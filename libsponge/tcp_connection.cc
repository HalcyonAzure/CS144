#include "tcp_connection.hh"

#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <iostream>
#include <type_traits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_tick - _receive_tick; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // 记录当前的时间
    _receive_tick = _time_tick;

    // 处理带有RST字段的报文
    if (seg.header().rst) {
        _is_active = false;
        // 将输入流和输出流都处理为error状态
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        // 然后在这里永久的关闭链接
        // TODO
        return;
    }

    // 对数据包进行处理，在这一步中，如果是服务端，会接收客户端发送过来的数据并对数据进行拼装
    // 如果是客户端，则会收到服务器对之前发送报文的确认序号ackno和服务器当前的窗口大小window_size
    _receiver.segment_received(seg);

    // 接下来就是sender对ack和windows_size进行处理和确认，并根据ackno和window_size对窗口进行填充
    _sender.ack_received(_receiver.ackno().value(), _receiver.window_size());
    _sender.fill_window();

    // 对于SYN/FIN的确认，单独用一个空报文来实现
    if (seg.header().ack && (seg.header().syn || seg.header().fin)) {
        _sender.send_empty_segment();
    }

    // 如果ackno的值是存在的，传入的报文本身不消耗序列号，并且传来的ackno是错误的，则返回一个空报文
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == _receiver.ackno().value() - 1) {
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
}

void TCPConnection::end_input_stream() {
    // 接收端已经接受了对方传来的所有数据，包括结束符号
    // 同时发送端也已经将自己所有的数据，包括eof发送完毕，同时被对方确认了
    if (_receiver.stream_out().eof() && _sender.stream_in().eof() && _sender.next_seqno() == _receiver.ackno()) {
        _is_active = false;
    }
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
    // 将TCP Sender的队列报文发送出去，并且检测是否存在ackno，如果存在的话则将ACK = 1
    // 确认报文只需要确认一次
    if (_receiver.ackno().has_value()) {
        _sender.segments_out().front().header().ack = true;
        _sender.segments_out().front().header().ackno = _receiver.ackno().value();
    }
    _segments_out.swap(_sender.segments_out());
}