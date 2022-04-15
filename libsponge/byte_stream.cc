#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t written_len = min(remaining_capacity(), data.size());
    _buffer.push_back(move(string().assign(data.begin(), data.begin() + written_len)));
    _bytes_written += written_len;
    return written_len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_len = min(len, buffer_size());
    string peek_str;
    // Resize the capacity of peek_str
    peek_str.reserve(peek_len);

    for (const auto &buf : _buffer) {
        if (peek_len >= buf.size()) {
            peek_str.append(buf);
            peek_len -= buf.size();
        } else {
            string buf_cpy = buf.copy();
            peek_str.append(buf_cpy.begin(), buf_cpy.begin() + peek_len);
            break;
        }
    }
    return peek_str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_len = min(len, buffer_size());
    if (pop_len <= 0) {
        return;
    }
    _bytes_read += pop_len;
    while (pop_len > 0) {
        if (pop_len > _buffer.front().size()) {
            pop_len -= _buffer.front().size();
            _buffer.pop_front();
        } else {
            _buffer.front().remove_prefix(pop_len);
            pop_len = 0;
        }
    }
    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t read_len = min(len, buffer_size());
    string read_str = peek_output(read_len);
    pop_output(read_len);
    return read_str;
}

void ByteStream::end_input() { _end = true; }

bool ByteStream::input_ended() const { return _end; }

size_t ByteStream::buffer_size() const { return _bytes_written - _bytes_read; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _end && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
