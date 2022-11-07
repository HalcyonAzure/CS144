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
    // expand_size: 扩容大小和窗口滑动最大值有关
    size_t expand_size = write_p + _output.remaining_capacity();

    cache.resize(expand_size);
    dirty_check.resize(expand_size);

    // 记录EOF的位置
    if (eof && index + data.length() <= expand_size) {
        end_p = index + data.length();
    }

    // 扩容只会变大，不会缩小,且扩容的容量不会超过_capacity
    // 最大扩容位置为 write_p + remaining_capacity
    // if (extend_size > cache.length()) {
    //     cache.resize(extend_size);
    //     dirty_check.resize(extend_size);
    // }

    // 将要排序的内容写入cache当中
    cache.replace(index, data.length(), data);
    dirty_check.replace(index, data.length(), data.length(), '1');

    // 检查写入位上是否有字符，有字符则通过滑动len来写入_output，否则跳过
    if (dirty_check[write_p]) {
        size_t len = 0;
        size_t output_remaining = _output.remaining_capacity();
        while (dirty_check[write_p + len] && len < output_remaining) {
            len++;
        }
        _output.write(cache.substr(write_p, len));
        write_p += len;
    }

    // 写入位和EOF位相同，代表写入结束
    if (write_p == end_p) {
        _output.end_input();
    }
}

// 返回缓冲区内还没有处理的内容
size_t StreamReassembler::unassembled_bytes() const {
    size_t n = write_p;
    while (not dirty_check[n] && n != cache.length()) {
        n++;
    }
    return cache.length() - n;
}

// 当不再写入新的TCP段并且已有的字段全部排序结束的时候缓冲区不再需要排序
bool StreamReassembler::empty() const { return _output.eof() && not unassembled_bytes(); }
