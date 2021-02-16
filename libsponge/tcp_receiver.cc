#include "tcp_receiver.hh"
#include <iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    size_t abs_seqno = 0;
    size_t length;
    if (seg.header().syn) {
        if (_syn_flag) return false;
        _syn_flag = true;
        _base = 1;
        _isn = seg.header().seqno;
        ret = true;
        abs_seqno = 1;
        length = seg.length_in_sequence_space() - 1;
        if (length == 0) return true;
    } else if (!_syn_flag) {
        return false;
    } else {
        // valid seg, 
        abs_seqno = unwrap(seg.header().seqno, _isn, abs_seqno);
        length = seg.length_in_sequence_space();
    }

    if (seg.header().fin) {
        if (_fin_flag) return false; // already fin
        _fin_flag = true;
        ret = true;
    } else if (seg.length_in_sequence_space() == 0 && _base == abs_seqno) {
        return true;
    } else if (abs_seqno >= _base + window_size() || abs_seqno + length <= _base) {
        if (!ret) return false;
    }

    _reassembler.push_substring(seg.payload().copy(), abs_seqno-1, seg.header().fin);
    _base = _reassembler.head_index() + 1;
    if (_reassembler.input_ended()) {
        _base++;
    }
    return true;

/*
    // set isn
    bool acceptable = false;
    TCPHeader header = seg.header();
    if (header.syn) {
        isnFlag = true;
        ackFlag = true;
        isn = header.seqno;
        ack = isn;
        acceptable = true;
        //checkpoint = static_cast<uint64_t>(isn.raw_value());
        //checkpoint = 0; 
    }
    if (header.fin) finFlag = true;
    // push data
    if (isnFlag) {    
        Buffer buffer = seg.payload();
        bool eof = header.fin;
//        uint64_t len = static_cast<uint64_t>(seg.length_in_sequence_space());
        uint64_t streamIdx; 
        if (header.syn) {
            streamIdx = unwrap(header.seqno, isn, checkpoint); // if syn, should +1
        } else {
            streamIdx = unwrap(header.seqno, isn, checkpoint) - 1;
        }
        cout << "checkpoint:" << checkpoint << "  streamIdx:" << streamIdx <<endl;
        _reassembler.push_substring(string(buffer.str()), streamIdx , eof);
        // determine if the data is in window 
        // the data is in [seqno, seqno+length_in_sequence_space() )
        // check if left_edge >= higher or right_edge <= lower (ackno)
        WrappingInt32 higher = ack + static_cast<uint32_t>(window_size());
        if (header.seqno.raw_value()>=higher.raw_value()||header.seqno.raw_value()+seg.length_in_sequence_space()<=ack.raw_value()){
        } else {
            acceptable = true;
        }	
        // update ack
        if (header.syn && header.fin) ack = ack + 2;
        else if (header.syn) ack = ack + 1;
        else if (header.seqno == ack && finFlag) ack = ack + 1;
        ack = ack + stream_out().bytes_written() - last; 
        last = static_cast<uint32_t>(stream_out().bytes_written());
        checkpoint += static_cast<uint64_t>(seg.length_in_sequence_space())-1;
     } else {
        return false;
    }
    return acceptable;
*/
    
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_base > 0) return wrap(_base, _isn);  
    return std::nullopt; 
}

size_t TCPReceiver::window_size() const {
    return stream_out().remaining_capacity();
}
