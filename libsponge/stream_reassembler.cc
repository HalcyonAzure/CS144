#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
    _cache.reserve(capacity);
    _dirty_check.reserve(capacity);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    bool eof_flag = false;
    size_t expanded_size = 0;

    // 取 index + data.length() 和
    // write_p + _output.remaining_capacity() 中更小的那个作为扩容后的大小
    if (index + data.length() <= writing_position + _output.remaining_capacity()) {
        // 用于判断EOF是否是在capacity当中的有效字符
        eof_flag = true;
        expanded_size = index + data.length();
    } else {
        expanded_size = writing_position + _output.remaining_capacity();
    }

    // 记录EOF的位置
    if (eof && eof_flag) {
        end_position = expanded_size;
    }

    const size_t real_size = _cache.length();
    bool need_expand = expanded_size > real_size;

    // 如果需要扩容则进行一次扩容
    _expand_cache(need_expand, expanded_size);

    // 将要排序的内容先写入cache当中，此时如果有多余的字符则会先填入缓冲区
    _cache.replace(index, data.length(), data);
    _dirty_check.replace(index, data.length(), data.length(), '1');

    // 这里是将缓冲区的长度恢复为写入cache之前的长度，来达到丢弃多余字符的目的
    _expand_cache(need_expand, expanded_size);

    // 检查写入位上是否有字符，有字符则通过滑动len来写入_output，否则跳过
    if (_dirty_check[writing_position] != 0) {
        size_t len = 0;
        size_t output_remaining = _output.remaining_capacity();
        while ((_dirty_check[writing_position + len] != 0) && len < output_remaining) {
            len++;
        }
        _output.write(_cache.substr(writing_position, len));
        writing_position += len;
    }

    // 写入位和EOF位相同，代表写入结束
    if (writing_position == end_position) {
        _output.end_input();
    }
}

// 返回缓冲区内还没有处理的内容
size_t StreamReassembler::unassembled_bytes() const {
    size_t cnt = 0;
    for (size_t i = writing_position; i < _cache.length(); i++) {
        if (_dirty_check[i] != 0) {
            cnt++;
        }
    }
    return cnt;
}

// 当不再写入新的TCP段并且已有的字段全部排序结束的时候缓冲区不再需要排序
bool StreamReassembler::empty() const { return _output.eof() && (unassembled_bytes() == 0); }

void StreamReassembler::_expand_cache(bool need_expand, size_t expanded_size) {
    if (need_expand) {
        _cache.resize(expanded_size);
        _dirty_check.resize(expanded_size);
    }
}