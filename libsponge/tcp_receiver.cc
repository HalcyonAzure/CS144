#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver
// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 在没开始握手的时候等待syn的信号，如果来了则建立链接，并且初始化isn
    if (not _is_syn && seg.header().syn) {
        _is_syn = true;
        _isn = seg.header().seqno;
        _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);
    } else if (not _is_syn) {
        return;
    }

    uint64_t stream_index = unwrap(seg.header().seqno, _isn, _checkpoint);
    if (stream_index != 0) {
        _reassembler.push_substring(seg.payload().copy(), stream_index - 1, seg.header().fin);
    }

    // 标志结尾的TCP段是否送达
    if (seg.header().fin) {
        _is_fin = true;
    }

    // 在push完新的字符流以后更新checkpoint的位置
    _checkpoint += seg.payload().size();
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    WrappingInt32 result = _isn + _is_syn + _reassembler.stream_out().bytes_written();
    if (_is_fin && _reassembler.unassembled_bytes() == 0) {
        result = result + _is_fin;
    }
    return _is_syn ? optional<WrappingInt32>(result) : nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
