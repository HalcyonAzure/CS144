#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _ackno; }

void TCPSender::fill_window() {
    while (_window_size != 0) {
        TCPSegment section;

        // 发送的数据包的序号是将要写入的下一个序号
        section.header().seqno = next_seqno();

        // _next_seqno == 0 代表还没有开始发送数据，此时需要发送SYN报文
        bool is_syn = (_next_seqno == 0);
        bool is_fin = _stream.input_ended();

        // 判断是否为SYN报文
        section.header().syn = is_syn;

        // 将数据进行封装
        size_t segment_payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, _window_size);
        section.payload() = _stream.read(segment_payload_size);

        // 空闲窗口中至少要留有一位序号的位置才能将当前数据包添加FIN(bytes_in_flight的也会占用窗口)
        if (is_fin && _window_size > (section.length_in_sequence_space() + bytes_in_flight())) {
            section.header().fin = true;
        }

        // 如果这个报文啥都没有，或者FIN报文已经被对方确认了，就不要发送了，代表连接全部结束了
        if (section.length_in_sequence_space() == 0 || _next_seqno == _stream.bytes_written() + 2) {
            return;
        }

        _send_segment(section);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \details 用于确认接收到的报文，更新发送端的状态
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    // 如果接收到对方发送的确认序号大于自己的下一个序号或者小于自己的已经被确认序号，说明接收到的确认序号是错误的
    if (abs_ackno > _next_seqno || abs_ackno < _ackno) {
        return;
    }
    _ackno = abs_ackno;

    // 记录对方的窗口是否满了
    _is_zero = (window_size == 0);

    // 如果对方的窗口大小是1，说明对方的窗口已经满了，需要等待对方的窗口释放后再发送数据
    _window_size = window_size ? window_size : 1;

    _rto_count = 0;  // 到达确认报文，重传次数清零

    // 如果当前缓冲区内有需要发送的报文，并且当前缓冲区内的第一个报文的序号等于接收到的确认序号，说明这个报文已经被对方确认了
    // if (not _cache.empty() &&
    //     _cache.front().header().seqno.raw_value() + _cache.front().length_in_sequence_space() == ackno.raw_value()) {
    //     _cache.pop();
    //     _has_segment_flight = false;
    //     _sent_tick = _current_tick;
    //     _rto_timeout = _initial_retransmission_timeout;
    // }

    // 当缓冲区内的报文已经被ackno确认，则将已经确认的报文进行丢弃
    while (not _cache.empty() &&
           _cache.front().header().seqno.raw_value() + _cache.front().length_in_sequence_space() <= ackno.raw_value()) {
        _cache.pop();
        _has_segment_flight = false;
        _sent_tick = _current_tick;
        _rto_timeout = _initial_retransmission_timeout;
    }

    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _current_tick += ms_since_last_tick;

    // 如果没有超时，或者当前缓冲区内没有需要发送的包
    if (_current_tick - _sent_tick < _rto_timeout || _cache.empty()) {
        return;
    }

    // 更新时间
    _sent_tick = _current_tick;

    // 如果上一个收到的报文中，窗口大小不是零，但是依旧超时，说明是网络堵塞
    if (not _is_zero) {
        _rto_count++;
        _rto_timeout *= 2;
    }

    // 重传次数小于最大重传次数，就重传
    if (_rto_count <= TCPConfig::MAX_RETX_ATTEMPTS) {
        _segments_out.push(_cache.front());
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _rto_count; }

void TCPSender::send_empty_segment() {
    TCPSegment empty_segment;
    empty_segment.header().seqno = next_seqno();
    segments_out().push(empty_segment);
}

// My Private Method
void TCPSender::_send_segment(const TCPSegment &seg) {
    // 当前报文需要占用的长度
    const size_t seg_len = seg.length_in_sequence_space();
    _window_size -= seg_len;
    _next_seqno += seg_len;
    _cache.push(seg);
    _segments_out.push(seg);
    // 为新发送的报文开启超时重传计时器
    if (not _has_segment_flight) {
        _rto_timeout = _initial_retransmission_timeout;
        _sent_tick = _current_tick;
        _has_segment_flight = true;
    }
}