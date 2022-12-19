#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const {
    // TODO
    return _time_tick - _receive_tick;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _receive_tick = _time_tick;
    // TODO: RST Segment
    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        return;
    }

    if (_sender.stream_in().input_ended()) {
        _close_tick = _time_tick;
    }

    _receiver.segment_received(seg);
    _sender.ack_received(seg.header().ackno, seg.header().win);
    _sender.fill_window();

    // Confirm SYN/FIN
    if (seg.header().ack && (seg.header().syn || seg.header().fin)) {
        _sender.send_empty_segment();
    }
    _push_out();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) { return _sender.stream_in().write(data); }

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_tick += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    // Clean shutdown
    if (_sender.stream_in().input_ended() && _time_tick - _close_tick >= 10 * _cfg.rt_timeout) {
        _linger_after_streams_finish = false;
        _is_active = false;
    }
    _push_out();
}

void TCPConnection::end_input_stream() {
    // Send FIN Segment
    _sender.stream_in().end_input();
    _sender.fill_window();
    _push_out();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _sender.segments_out().swap(_segments_out);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_push_out() {
    if (_sender.segments_out().empty()) {
        return;
    }
    if (_receiver.ackno().has_value()) {
        _sender.segments_out().front().header().ack = true;
        _sender.segments_out().front().header().ackno = _receiver.ackno().value();
    }
    _segments_out.swap(_sender.segments_out());
}