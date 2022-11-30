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
    if (_next_seqno == 0) {
        TCPSegment syn_signal;
        // ackno小于等于，说明连接没建立，每次都要携带syn
        syn_signal.header().syn = true;
        syn_signal.header().seqno = _isn;
        syn_signal.header().ack = (_ackno == _isn.raw_value() + 1);
        _send_segment(syn_signal);
        return;
    }

    if (_stream.eof() && _window_size != 0) {
        TCPSegment fin_signal;
        fin_signal.header().fin = true;
        _send_segment(fin_signal);
        return;
    }

    for (size_t i = 0; i < _window_size && not _stream.buffer_empty(); i += TCPConfig::MAX_PAYLOAD_SIZE) {
        size_t segment_payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, _window_size);

        TCPSegment section;

        section.payload() = _stream.read(segment_payload_size);
        section.header().seqno = wrap(_next_seqno, _isn);
        section.header().fin = _stream.input_ended();

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
    _window_size = window_size;
    while (not _cache_segments.empty() && _cache_segments.front().header().seqno != ackno) {
        _cache_segments.pop();
    }
    _consecutive_retransmissions = 0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _time_ticker += ms_since_last_tick;
    if (_time_ticker - _segment_ticker < _rto_ticker) {
        return;
    }
    _segment_ticker = _time_ticker;
    _consecutive_retransmissions++;
    _rto_ticker *= 2;
    _segments_out.push(_cache_segments.front());
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {}

// My Private Method
void TCPSender::_send_segment(TCPSegment seg) {
    _rto_ticker = _initial_retransmission_timeout;
    _segment_ticker = _time_ticker;
    _window_size -= seg.length_in_sequence_space();
    _next_seqno += seg.length_in_sequence_space();
    _cache_segments.push(seg);
    _segments_out.push(seg);
}