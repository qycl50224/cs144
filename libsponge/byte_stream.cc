#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) { 
	this->_capacity = capacity; 
}

size_t ByteStream::write(const string &data) {
    size_t len = data.size();
    if (len > _capacity - _buffer.size()) {
        len = _capacity - _buffer.size();
    }
    _write_count += len;
    string s;
    s.assign(data.begin(),data.begin()+len);
    _buffer.append(BufferList(move(s)));
    return len;
}

string ByteStream::peek_output(const size_t len) const {
    size_t length = len;
    if (length > _buffer.size()) length = _buffer.size();
    string s = _buffer.concatenate();
    return string().assign(s.begin(), s.begin()+length);
}

void ByteStream::pop_output(const size_t len) { 
    size_t length = len;
    if (length > _buffer.size()) length = _buffer.size();
    _buffer.remove_prefix(length);
	_read_count += length;   // from the testcase can we know that 
								// _read_count have to be updated here
}

std::string ByteStream::read(const size_t len) {
    string result = peek_output(len);
    pop_output(len);
    return result;
}

void ByteStream::end_input() {_end_write = true;}

bool ByteStream::input_ended() const { return {_end_write}; }

size_t ByteStream::buffer_size() const { return {_buffer.size()}; }

bool ByteStream::buffer_empty() const { return {_buffer.size() == 0}; }

bool ByteStream::eof() const { return {input_ended() && buffer_empty()}; }

size_t ByteStream::bytes_written() const { return {_write_count}; }

size_t ByteStream::bytes_read() const { return {_read_count}; }

size_t ByteStream::remaining_capacity() const { return {_capacity - _buffer.size()}; }
