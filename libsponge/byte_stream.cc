#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) { buffer_max_size = capacity; }

size_t ByteStream::write(const string &data) {
    this->input += data;
    size_t cnt = 0;
    while (!input_ended() && remaining_capacity()) {
        this->buffer.push_back(*input.begin());
        input.erase(input.begin());
        cnt++;
    }
    // 统计写入数据
    this->wcnt += cnt;
    return cnt;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_len = len;
    string peek_string;
    while (peek_len--) {
        string::const_iterator buffer_iter = this->buffer.begin();
        peek_string.push_back(*buffer_iter);
    }
    return peek_string;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t output_len = len;
    while (output_len--) {
        buffer.pop_back();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    this->output = buffer.substr(0, len);
    pop_output(len);
    return output;
}

void ByteStream::end_input() {}

// 测试input是否被完全读取
bool ByteStream::input_ended() const {
    if (this->input.size() == 0) {
        return true;
    }
    return false;
}

size_t ByteStream::buffer_size() const { return 3; }

// 缓冲区是否为空
bool ByteStream::buffer_empty() const { return this->buffer.empty(); }

bool ByteStream::eof() const { return this->input.empty(); }

size_t ByteStream::bytes_written() const { return this->wcnt; }

size_t ByteStream::bytes_read() const { return this->rcnt; }

size_t ByteStream::remaining_capacity() const { return (this->buffer_max_size - this->buffer.length()); }
