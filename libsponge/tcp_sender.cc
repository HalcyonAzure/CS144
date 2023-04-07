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
    size_t fill_space = _window_size ? _window_size : 1;
    fill_space -= bytes_in_flight();
    while (fill_space > 0) {
        TCPSegment section;

        // 发送的数据包的序号是将要写入的下一个序号
        section.header().seqno = next_seqno();

        // _next_seqno == 0 代表还没有开始发送数据，此时需要发送SYN报文
        section.header().syn = (_next_seqno == 0);

        // 将数据进行封装
        size_t segment_payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, fill_space);
        section.payload() = _stream.read(segment_payload_size);

        // 空闲窗口中至少要留有一位序号的位置才能将当前数据包添加FIN(bytes_in_flight的也会占用窗口)
        if (_stream.eof() && fill_space > section.length_in_sequence_space()) {
            section.header().fin = true;
        }

        // 如果这个报文啥都没有，或者FIN报文已经发送了，就没必要发送新的数据段了
        if (section.length_in_sequence_space() == 0 || _next_seqno == _stream.bytes_written() + 2) {
            return;
        }

        fill_space -= section.length_in_sequence_space();

        _send_segment(section);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \details 用于确认接收到的报文，更新发送端的状态
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    // 如果接收到对方发送的确认序号大于自己的下一个序号或者小于自己的已经被确认序号，说明接收到的确认序号是错误的
    if (abs_ackno < _ackno || abs_ackno > _next_seqno) {
        return;
    }
    _ackno = abs_ackno;

    // 记录窗口大小
    _window_size = window_size;

    // 用于判断是否重置计时器
    bool has_reset = false;

    // 当缓冲区内的报文已经被ackno确认，则将已经确认的报文进行丢弃
    while (not _cache.empty() &&
           _cache.front().header().seqno.raw_value() + _cache.front().length_in_sequence_space() <= ackno.raw_value()) {
        if (not has_reset) {
            // 有效的确认报文到达，重置计时器
            _rto_timer.reset(_initial_retransmission_timeout);
            has_reset = true;
        }
        _cache.pop();
    }

    if (_cache.empty()) {
        // 所有数据包都被确认了，所以暂停计时器
        _rto_timer.stop();
    }

    // 如果剩余的窗口还有空间，就填入内容
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 更新当前时间
    _rto_timer.update(ms_since_last_tick);

    // 检测是否超时
    if ((not _rto_timer.is_timeout())) {
        return;
    }
    // 如果上一个收到的报文中，窗口大小不是零，但是依旧超时，说明是网络堵塞，执行慢启动
    if (_window_size != 0) {
        _rto_timer.slow_start();
    }

    // 重传次数小于最大重传次数，就重传
    if (_rto_timer.rto_count() <= TCPConfig::MAX_RETX_ATTEMPTS) {
        // 发送缓冲区中的第一个报文段
        _segments_out.push(_cache.front());
        _rto_timer.restart();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _rto_timer.rto_count(); }

void TCPSender::send_empty_segment() {
    TCPSegment empty_segment;
    empty_segment.header().seqno = next_seqno();
    segments_out().push(empty_segment);
}

// 先将数据段存入缓存中，然发送出去
void TCPSender::_send_segment(const TCPSegment &seg) {
    // 当前报文需要占用的长度
    const size_t seg_len = seg.length_in_sequence_space();
    _next_seqno += seg_len;
    _cache.push(seg);
    _segments_out.push(seg);
    // 如果没启动计时器，就启动计时器
    if (not _rto_timer.is_running()) {
        _rto_timer.run();
        _rto_timer.reset(_initial_retransmission_timeout);
    }
}