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
    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _is_active = false;
        return;
    }

    if (not seg.header().syn && (_sender.next_seqno_absolute() == 0 || not _receiver.ackno().has_value())) {
        return;
    }

    _receiver.segment_received(seg);
    _sender.ack_received(seg.header().ackno, seg.header().win);
    _sender.fill_window();

    if (_receiver.stream_out().eof() && not _sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    if (_sender.stream_in().eof() && bytes_in_flight() == 0 && not _linger_after_streams_finish) {
        _is_active = false;
    }

    if (_sender.segments_out().empty() &&
        (seg.header().fin || seg.header().syn || seg.header().seqno != _receiver.ackno())) {
        _sender.send_empty_segment();
    }
    _push_out();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    size_t written_cnt = _sender.stream_in().write(data);
    _sender.fill_window();
    _push_out();
    return written_cnt;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_tick += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _error_with_rst();
        return;
    }
    // 超过十倍的超时等待时间，代表连接主动结束，双方数据都结束发送
    if (time_since_last_segment_received() >= _linger_time && _sender.stream_in().eof()) {
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
    _push_out();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _error_with_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_push_out() {
    if (_sender.segments_out().empty()) {
        return;
    }
    while (not _sender.segments_out().empty()) {
        if (_receiver.ackno().has_value()) {
            _sender.segments_out().front().header().ack = true;
            _sender.segments_out().front().header().ackno = _receiver.ackno().value();
            _sender.segments_out().front().header().win = _receiver.window_size();
        }
        _segments_out.push(_sender.segments_out().front());
        _sender.segments_out().pop();
    }
}

void TCPConnection::_error_with_rst() {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _is_active = false;
    _sender.send_empty_segment();
    _sender.segments_out().front().header().rst = true;
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();
}