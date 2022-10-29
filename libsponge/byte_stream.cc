#include "byte_stream.hh"

#include <iostream>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : buffer_max_size(capacity), buffer(""), input(""), wcnt(0), rcnt(0) {}

size_t ByteStream::write(const string &data) {
    cout << "Before Write:" << buffer << " with Data:" << data << endl;
    this->input = data;
    cout << "Input: " << input << endl;
    size_t cnt = 0;
    while (!input_ended() && remaining_capacity() && this->input.length()) {
        this->buffer.push_back(*input.begin());
        input.erase(0, 1);
        cnt++;
    }
    // 统计写入数据
    this->wcnt += cnt;
    cout << "After Write:" << buffer << endl;
    return cnt;
    // DUMMY_CODE(data);
    // return {};
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_len = len;
    string peek_string;
    string::const_iterator buffer_iter = this->buffer.begin();
    while (peek_len--) {
        peek_string.push_back(*buffer_iter);
        buffer_iter++;
    }
    return peek_string;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    cout << "Before Pop:" << buffer << " Pop len:" << len << endl;
    buffer.erase(0, len);
    cout << "After Pop:" << buffer << endl;
    this->rcnt += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string output = buffer.substr(0, len);
    pop_output(len);
    this->rcnt += len;
    return output;
}

void ByteStream::end_input() {
    isInputEnd = true;
    input.clear();
}

bool ByteStream::input_ended() const { return this->isInputEnd; }

size_t ByteStream::buffer_size() const { return buffer.length(); }

// 缓冲区是否为空
bool ByteStream::buffer_empty() const { return this->buffer.empty(); }

bool ByteStream::eof() const { return input_ended() && this->buffer.empty(); }

size_t ByteStream::bytes_written() const { return this->wcnt; }

size_t ByteStream::bytes_read() const { return this->rcnt; }

size_t ByteStream::remaining_capacity() const { return (this->buffer_max_size - this->buffer.length()); }
