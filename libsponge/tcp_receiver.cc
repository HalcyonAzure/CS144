#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver
// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 等待并处理第一个syn链接
    if ((_is_syn == 0) && seg.header().syn) {
        _is_syn = 1;
        _isn = seg.header().seqno;
    } else if (_is_syn == 0) {
        return;
    }

    // checkpoint的位置就是已经写入完成的字符的数量
    // In your TCP implementation, you’ll use the index of the last reassembled byte as the checkpoint.
    const uint64_t checkpoint = _reassembler.stream_out().bytes_written() + 1;

    // 将内容写入reassembler，判断!=0的原因是为了忽略那些小于等于seq的错误TCP段
    uint64_t stream_index = unwrap(seg.header().seqno, _isn, checkpoint) - 1 + seg.header().syn;
    _reassembler.push_substring(seg.payload().copy(), stream_index, seg.header().fin);

    // 标志结尾的TCP段是否送达
    if (seg.header().fin) {
        _is_fin = 1;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    WrappingInt32 result = _isn + _is_syn + _reassembler.stream_out().bytes_written();
    if ((_is_fin != 0) && _reassembler.unassembled_bytes() == 0) {
        result = result + _is_fin;
    }
    return _is_syn != 0 ? optional<WrappingInt32>(result) : nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
