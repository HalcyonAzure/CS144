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
    return _time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // 首先无论如何，刷新收到报文的时间
    _time_since_last_segment_received = 0;

    // 然后先检查这个报文是否出错，如果出错则直接返回
    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _is_active = false;
        return;
    }

    // 如果TCP连接处于LISTEN状态，只接受SYN报文，并且返回一个SYN + ACK的报文
    if (not _receiver.ackno().has_value()) {
        if (seg.header().syn) {
            _sender.fill_window();
        } else {
            return;
        }
    }

    // 接受这个报文
    _receiver.segment_received(seg);

    // 对ACK报文进行确认更新，用于下一次更新的确认
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    // 接收到正确的EOF报文，代表对方发送过来的数据流已经结束了，但是自己还有数据要发送
    // 因此需要等待自己的数据流发送完毕后才能关闭连接
    if (_receiver.stream_out().eof() && not _sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    // _linger_after_streams_finish是false说明对方发送给我们的数据流已经全部被接受了
    // 此时有_sender的eof和bytes_in_flight都为0，说明自己的数据流也已经全部发送完毕
    // 因此可以关闭连接了
    if (_sender.stream_in().eof() && bytes_in_flight() == 0 && not _linger_after_streams_finish) {
        _is_active = false;
    }

    if (_sender.segments_out().empty() && (seg.length_in_sequence_space() || seg.header().seqno != _receiver.ackno())) {
        _sender.send_empty_segment();
    }

    // 填装需要发送的报文
    _push_out();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    // Send Data Segment
    size_t written_cnt = _sender.stream_in().write(data);
    _sender.fill_window();
    _push_out();
    return written_cnt;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    // 如果超时重传次数超过了最大重传次数，那么就直接关闭连接
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _send_rst();
        return;
    }
    // 在我方的数据包全部发送并且处理完毕以后，如果接受到了对方传来的EOF报文，并且等待了十倍的RTT时间都没有新的报文传来，则代表连接已经关闭
    if (time_since_last_segment_received() >= _linger_time && _sender.stream_in().eof() &&
        _receiver.stream_out().input_ended()) {
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
    // Send SYN Segment
    _sender.fill_window();
    _push_out();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_push_out() {
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

void TCPConnection::_send_rst() {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _is_active = false;

    // Send RST Segment
    TCPSegment rst_segment;
    rst_segment.header().rst = true;
    _segments_out.push(rst_segment);
}