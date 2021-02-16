#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <set>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
using namespace std;

class StreamReassembler {
    private:
    // Your code here -- add private members as necessary.

    struct Segment{
        string data = "";
        size_t start = 0;  // [start, end]
        size_t end = 0;
        size_t size = 0;
        bool eof = false;
        Segment() {}
        Segment(string &dataa, size_t indexx, bool eoff) :
        data(dataa), start(indexx), size(dataa.length()), eof(eoff)
        {
            if (dataa.size() == 0) end = start;
            else end = indexx+dataa.length()-1;
        }    // move?
        bool operator < (const Segment &s) const{
            return  this->start < s.start;
        }
    };

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    std::set<Segment> _data{};
    size_t lookfor = 0;
    size_t _unassembled_bytes = 0;
    bool _eof = false;

    bool existedSeg(Segment s) {
        // if already existed
        for(set<Segment>::iterator it = _data.begin() ;it != _data.end();)
        {
            if(contain(*it, s)) {
                return true;
            }else{
                it++;
            }
        }
        return false;
    }

    bool contain(const Segment &a, const Segment &b) {
        int startA, endA, startB, endB;
        startA = a.start;
        startB = b.start;
        endA = a.end;
        endB = b.end;
        if (startA <= startB && endA >= endB) return true;
        return false;
    }

    // merge all connected segment to seg
    void coalesce(Segment &seg) {
        
        auto iter = _data.lower_bound(seg);

        while (iter != _data.end()) {
            if (seg.end + 1 >= iter->start) {
                // connect iter to seg
                string s = iter->data.substr(1 + seg.end - iter->start);
                string newdata = seg.data+s;
                seg = Segment(newdata, seg.start, iter->eof);
                // delete iter
                _unassembled_bytes -= iter->size;
                _data.erase(iter);
                // update iter
                iter = _data.lower_bound(seg);
            } else {
                break;
            }
        }

        if (iter == _data.begin()) {
            return;
        }
        iter--;
        while (iter != _data.begin()) {
            if (iter->end + 1 >= seg.start) {
                string s = seg.data.substr(1 + iter->end - seg.start);
                string newdata = iter->data+s;
                seg = Segment(newdata, iter->start, seg.eof);
                _unassembled_bytes -= iter->size;
                _data.erase(iter);
                iter = _data.lower_bound(seg);
                iter--;
            } else {
                break;
            }
        }
        // should do it once more to cover _data.begin()
        if (iter->end + 1 >= seg.start) {
                string s = seg.data.substr(1 + iter->end - seg.start);
                string newdata = iter->data+s;
                seg = Segment(newdata, iter->start, seg.eof);
                _unassembled_bytes -= iter->size;
                _data.erase(iter);
                iter = _data.lower_bound(seg);
        }
    }

    // add those segment whose start == lookfor to _output
    void addToStream() {
        if (!_data.empty() && _data.begin()->start == lookfor) {
            size_t writes = _output.write(_data.begin()->data);
            lookfor += writes;
            _unassembled_bytes -= writes;
            _data.erase(_data.begin());
        }
    }

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;


    // access helper
    size_t head_index() const {return lookfor;}
    bool input_ended() const {return _output.input_ended();}

};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
