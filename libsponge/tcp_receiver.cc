#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver
#include <iostream>
// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 在没开始握手的时候等待syn的信号，如果来了则建立链接，并且初始化isn
    if (not is_connect && seg.header().syn == 1) {
        is_connect = true;
        isn = seg.header().seqno;
        ackno_to_send = isn;
    } else if (not is_connect) {
        return;
    }

    uint64_t index = unwrap(seg.header().seqno, isn, checkpoint);

    if (index != 0) {
        _reassembler.push_substring(seg.payload().copy(), index - 1, seg.header().fin);
    } else if (seg.header().fin) {
        _reassembler.stream_out().end_input();
    }

    checkpoint += seg.payload().size();

    if (seg.header().syn == 1) {
        ackno_to_send = ackno_to_send + 1;
    }
    if (seg.header().fin == 1) {
        ackno_to_send = ackno_to_send + 1;
    }

    ackno_to_send = ackno_to_send + seg.payload().size();
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    return is_connect ? optional<WrappingInt32>(ackno_to_send) : nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
