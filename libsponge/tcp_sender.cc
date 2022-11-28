#include "tcp_sender.hh"

#include "buffer.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <iostream>
#include <random>
#include <string>

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
    if (_window_size == 1) {
        TCPSegment signal;
        if (_next_seqno == 0) {
            // ackno小于等于，说明连接没建立，每次都要携带syn
            signal.header().syn = true;
            signal.header().seqno = _isn;
            if (_ackno == _isn.raw_value() + 1) {
                signal.header().ack = true;
            }
        }
        // next seqno 消耗了length_in_sequence_space的字符
        _next_seqno += signal.length_in_sequence_space();
        _segments_out.push(signal);
        return;
    }

    if (_stream.buffer_empty()) {
        return;
    }

    _cache = _stream.read(_window_size);
    for (size_t i = 0; i < _window_size; i += TCPConfig::MAX_PAYLOAD_SIZE) {
        size_t segment_payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, _window_size);
        TCPSegment section;
        string section_string = _cache.substr(i, segment_payload_size);
        section.payload() = _cache.substr(i, segment_payload_size);
        section.header().seqno = wrap(_next_seqno, _isn);
        _next_seqno += section_string.length();
        _segments_out.push(section);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _ackno = unwrap(ackno, _isn, _next_seqno);
    _window_size = window_size;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { _time_ticker += ms_since_last_tick; }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {
    // 当窗口值为 1 的时候发送数据
    TCPSegment signal;
    signal.header().syn = true;
    if (_ackno == _isn.raw_value() + 1) {
        signal.header().ack = true;
    }
    signal.header().seqno = _isn;
    _next_seqno += signal.length_in_sequence_space();
    _segments_out.push(signal);
}
