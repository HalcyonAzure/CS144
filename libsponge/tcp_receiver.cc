#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver
// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 在没开始握手的时候等待syn的信号，如果来了则建立链接，并且初始化isn
    if (not _is_connect && seg.header().syn == 1) {
        _is_connect = true;
        _isn = seg.header().seqno;
        if (seg.payload().size() >= 1) {
            _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);
        }
    } else if (not _is_connect) {
        return;
    }

    if (seg.header().fin == 1) {
        _is_fin = true;
    }

    uint64_t index = unwrap(seg.header().seqno, _isn, _checkpoint);

    if (index != 0) {
        _reassembler.push_substring(seg.payload().copy(), index - 1, seg.header().fin);
    }
    _ackno = _isn + _reassembler.stream_out().bytes_written() + 1;

    _checkpoint += seg.payload().size();

    if (_is_fin && _reassembler.unassembled_bytes() == 0) {
        _ackno = _ackno + 1;
        _reassembler.stream_out().end_input();
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { return _is_connect ? optional<WrappingInt32>(_ackno) : nullopt; }

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
