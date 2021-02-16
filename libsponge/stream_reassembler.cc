#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {} 

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
	
}

 
//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
	// if index > lookfor + max buffer
	if (index >= lookfor + _capacity) return;
	// if empty segment return
	else if (!eof && data.size()==0) {
		return;
	} 
	else if (!eof && index + data.size() - 1 < lookfor) {
		return;
	}

	// if index < lookfor, trim it
	size_t start = index;
	string useful_data;
	if (index < lookfor) {
		start = lookfor;
		useful_data = data.substr(lookfor - index);
	} else {
		useful_data = data;
	}
    
    // add data to _data
    StreamReassembler::Segment s(useful_data, start, eof);

    // check if some segment has covered s
	if (existedSeg(s)) {
		addToStream();
	}
	else {
		// delete some segments that have been covered by s
		for(set<Segment>::iterator it = _data.begin() ;it != _data.end();)
	    {
	        if(contain(s, *it))
	        {
	        	_unassembled_bytes -= it->size;
	            it = _data.erase(it); // pretty elegent way to delete
	        }else{
	            it++;
	        }
	    }
		coalesce(s);
		_data.emplace(s);
		_unassembled_bytes += s.size;
		addToStream();	
	}
	if (eof) {
		_eof = true;
	}
	if (_eof && empty()) {
		_output.end_input();
	}
	return;
	


}

size_t StreamReassembler::unassembled_bytes() const {
	return _unassembled_bytes;
}

bool StreamReassembler::empty() const {
	return _unassembled_bytes == 0;
} 
