#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
    cache.reserve(capacity);
    dirty_check.reserve(capacity);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    bool eof_flag = false;
    size_t expand_size = 0;

    // 取 index + data.length() 和
    // write_p + _output.remaining_capacity() 中更小的那个作为扩容后的大小
    if (index + data.length() <= write_p + _output.remaining_capacity()) {
        // 用于判断EOF是否是在capacity当中的有效字符
        eof_flag = true;
        expand_size = index + data.length();
    } else {
        expand_size = write_p + _output.remaining_capacity();
    }

    // 记录EOF的位置
    if (eof && eof_flag) {
        end_p = expand_size;
    }

    const size_t cache_raw_length = cache.length();

    // 先扩大一次容量，用于写入多余的内容
    if (expand_size > cache_raw_length) {
        cache.resize(expand_size);
        dirty_check.resize(expand_size);
    }

    // 将要排序的内容先写入cache当中
    cache.replace(index, data.length(), data);
    dirty_check.replace(index, data.length(), data.length(), '1');

    // 缩回原来的大小，将缓冲区外多余的内容丢弃
    if (expand_size > cache_raw_length) {
        cache.resize(expand_size);
        dirty_check.resize(expand_size);
    }

    // 检查写入位上是否有字符，有字符则通过滑动len来写入_output，否则跳过
    if (dirty_check[write_p] != 0) {
        size_t len = 0;
        size_t output_remaining = _output.remaining_capacity();
        while ((dirty_check[write_p + len] != 0) && len < output_remaining) {
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
    size_t cnt = 0;
    for (size_t i = write_p; i < cache.length(); i++) {
        if (dirty_check[i] != 0) {
            cnt++;
        }
    }
    return cnt;
}

// 当不再写入新的TCP段并且已有的字段全部排序结束的时候缓冲区不再需要排序
bool StreamReassembler::empty() const { return _output.eof() && (unassembled_bytes() == 0); }
