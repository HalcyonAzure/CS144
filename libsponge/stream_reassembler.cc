#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : cache(), dirty_check(), _output(capacity), _capacity(capacity) {
    cache.reserve(capacity);
    dirty_check.reserve(capacity);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // extend_size: 按照index和data.length()扩容后的大小，只会按扩大的来扩容
    size_t extend_size = index + data.length();

    if (eof) {
        end_pos = extend_size;
    }

    if (extend_size > cache.length()) {
        cache.resize(extend_size);
        dirty_check.resize(extend_size);
    }

    cache.replace(index, data.length(), data);
    dirty_check.replace(index, data.length(), data.length(), '1');

    if (dirty_check[write_pos]) {
        size_t len = 0;
        size_t output_remaining = _output.remaining_capacity();
        while (dirty_check[write_pos + len] && len < output_remaining) {
            len++;
        }
        _output.write(cache.substr(write_pos, len));
        write_pos += len;
    }

    if (write_pos == end_pos) {
        _output.end_input();
    }
}

// 返回缓冲区内还没有处理的内容
size_t StreamReassembler::unassembled_bytes() const {
    size_t n = 0;
    for (auto i = write_pos; i != dirty_check.length(); i++) {
        if (dirty_check[i])
            n++;
    }
    return n;
}

// 当输入结束并且所有数字都排序的时候代表缓冲区结束
bool StreamReassembler::empty() const { return _output.eof() && not unassembled_bytes(); }
