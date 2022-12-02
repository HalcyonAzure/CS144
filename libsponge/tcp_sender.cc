#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
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

        if (is_syn) {
            bool is_syn_acked = is_syn && (_ackno == _isn.raw_value() + 1);
            section.header().syn = true;
            section.header().ack = is_syn_acked;
        }

        if (not is_fin && not is_syn && _stream.buffer_empty()) {
            return;
        }

        size_t segment_payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, _window_size);
        section.payload() = _stream.read(segment_payload_size);

        if (is_fin && _window_size > section.length_in_sequence_space()) {
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
    if (unwrap_ackno > _next_seqno) {
        return;
    }
    _ackno = unwrap_ackno;
    _window_size = window_size ? window_size : 1;
    _consecutive_retransmissions = 0;
    while (not _cache_segments.empty() && _cache_segments.front().header().seqno != ackno) {
        _cache_segments.pop();
        _is_front = false;
        _segment_ticker = _time_ticker;
        _rto_ticker = _initial_retransmission_timeout;
        return;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _time_ticker += ms_since_last_tick;
    if (_time_ticker - _segment_ticker < _rto_ticker) {
        return;
    }
    _segment_ticker = _time_ticker;
    if (_window_size != 0 || _next_seqno == 1) {
        _consecutive_retransmissions++;
        _rto_ticker *= 2;
    }
    _segments_out.push(_cache_segments.front());
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {}

// My Private Method
void TCPSender::_send_segment(const TCPSegment &seg) {
    _rto_ticker = _initial_retransmission_timeout;
    if (not _is_front) {
        _segment_ticker = _time_ticker;
        _is_front = true;
    }
    _window_size -= seg.length_in_sequence_space();
    _next_seqno += seg.length_in_sequence_space();
    _cache_segments.push(seg);
    _segments_out.push(seg);
}