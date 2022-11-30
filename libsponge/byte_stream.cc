#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : buffer_max_size(capacity), write_count(0), read_count(0) {}

size_t ByteStream::write(const string &data) {
    size_t cnt = min(remaining_capacity(), data.length());
    buffer += data.substr(0, cnt);
    write_count += cnt;
    return cnt;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const { return buffer.substr(0, len); }

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    buffer.erase(0, len);
    read_count += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string output = buffer.substr(0, len);
    pop_output(len);
    return output;
}

void ByteStream::end_input() { is_input_end = true; }

bool ByteStream::input_ended() const { return is_input_end; }

size_t ByteStream::buffer_size() const { return buffer.length(); }

bool ByteStream::buffer_empty() const { return buffer.empty(); }

bool ByteStream::eof() const { return input_ended() && buffer.empty(); }

size_t ByteStream::bytes_written() const { return write_count; }

size_t ByteStream::bytes_read() const { return read_count; }

size_t ByteStream::remaining_capacity() const { return buffer_max_size - buffer.length(); }
