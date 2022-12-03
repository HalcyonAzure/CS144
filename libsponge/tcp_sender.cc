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

        section.header().seqno = wrap(_next_seqno, _isn);

        bool is_syn = (_next_seqno == 0);
        bool is_fin = _stream.input_ended();

        // 判断是否为SYN后的确认报文
        if (is_syn) {
            bool is_syn_acked = is_syn && (_ackno == _isn.raw_value() + 1);
            section.header().syn = true;
            section.header().ack = is_syn_acked;
        }

        // 只有FIN和SYN的报文可以是空字符
        if (not is_fin && not is_syn && _stream.buffer_empty()) {
            return;
        }

        size_t segment_payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, _window_size);
        section.payload() = _stream.read(segment_payload_size);

        // 如果要发送FIN的话，窗口内至少还要剩余一个字符
        if (is_fin && _window_size > section.length_in_sequence_space()) {
            // 如果已经发送过FIN，或者当前窗口还在处理FIN之前的数据，没有空余的窗口留给FIN的话则先跳过
            if (_next_seqno == _stream.bytes_written() + 2 || _window_size <= bytes_in_flight()) {
                return;
            }
            section.header().fin = true;
        }

        _send_segment(section);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t unwrap_ackno = unwrap(ackno, _isn, _next_seqno);
    if (unwrap_ackno > _next_seqno || unwrap_ackno < _ackno) {
        return;
    }
    _ackno = unwrap_ackno;

    // 记录是否为空窗口
    _is_zero = (window_size == 0);

    // 将0视为1
    _window_size = window_size ? window_size : 1;
    _rto_count = 0;
    while (not _cache.empty() &&
           _cache.front().header().seqno.raw_value() + _cache.front().length_in_sequence_space() == ackno.raw_value()) {
        _cache.pop();
        _rto_trigger = false;
        _sent_tick = _current_tick;
        _rto_tick = _initial_retransmission_timeout;
        return;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _current_tick += ms_since_last_tick;
    if (_current_tick - _sent_tick < _rto_tick || _cache.empty()) {
        return;
    }
    _sent_tick = _current_tick;
    if (not _is_zero) {
        _rto_count++;
        _rto_tick *= 2;
    }
    _segments_out.push(_cache.front());
}

unsigned int TCPSender::consecutive_retransmissions() const { return _rto_count; }

void TCPSender::send_empty_segment() {}

// My Private Method
void TCPSender::_send_segment(const TCPSegment &seg) {
    // 每次发送新报文的时候重置RTO超时最大时间
    _rto_tick = _initial_retransmission_timeout;
    if (not _rto_trigger) {
        _sent_tick = _current_tick;
        _rto_trigger = true;
    }
    // 当前报文需要占用的长度
    const size_t seg_len = seg.length_in_sequence_space();
    _window_size -= seg_len;
    _next_seqno += seg_len;
    _cache.push(seg);
    _segments_out.push(seg);
}