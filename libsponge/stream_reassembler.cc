#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : cache(), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    cache.resize(index + data.length() + 1);
    cache.replace(index, data.length(), data);

    if (cache[out_pos]) {
        size_t len = 0;
        while (cache[out_pos + len]) {
            len++;
        }
        _output.write(cache.substr(out_pos, len));
        out_pos += len;
    }

    if (eof) {
        _output.end_input();
    }
}

// 返回缓冲区内还没有处理的内容
size_t StreamReassembler::unassembled_bytes() const { return cache.length() - _output.buffer_size(); }

// 当输入结束并且所有数字都排序的时候代表缓冲区结束
bool StreamReassembler::empty() const { return _output.eof() && not unassembled_bytes(); }
