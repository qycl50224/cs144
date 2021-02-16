#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return {_sender.stream_in().remaining_capacity()}; }

size_t TCPConnection::bytes_in_flight() const { return {_sender.bytes_in_flight()}; }

size_t TCPConnection::unassembled_bytes() const { return {_receiver.unassembled_bytes()}; }

size_t TCPConnection::time_since_last_segment_received() const { return {_time_since_last_segment_received};}

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!active()) {
        return;
    }
    _time_since_last_segment_received = 0;

    // in syn_sent, get valid seg which means ack and payload() != 0, return
    if (in_syn_sent()) {
        if (seg.header().ack && seg.payload().size() != 0) {
            return;
        }
    }

    bool send_empty = false;
    if (_sender.next_seqno_absolute() > 0 && seg.header().ack) {
        // if ackno is invalid
        if (!_sender.ack_received(seg.header().ackno, seg.header().win)) {
            send_empty = true;
        }
    }

    // if !receiver.segment_received(seg) 
    if (!_receiver.segment_received(seg)) {
        send_empty = true;
    }

    // server connect to client
    if (seg.header().syn && _sender.next_seqno_absolute() == 0) {
        connect();
        return;
    }

    // if seg.length_in_sequence_space() > 0
    if (seg.length_in_sequence_space() > 0) {
        send_empty = true;
    }

    // if seg.rst
    if (seg.header().rst == true) {
        if (in_syn_sent() && !seg.header().ack) {
            return;
        }
        unclean_shutdown(false);
        return;
    }
    
    if (send_empty) {
        // the ackno is not recognized
        if (_receiver.ackno().has_value() && _sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
    }
    push_segments_out();
}

bool TCPConnection::active() const { return {_active}; }

size_t TCPConnection::write(const string &data) {
    size_t ret = _sender.stream_in().write(data); 
    push_segments_out();
    return ret; 
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active) {
        return;
    }
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_shutdown(true);
    }
    push_segments_out();
}

void TCPConnection::end_input_stream() {
        _sender.stream_in().end_input();
        push_segments_out();
    }

void TCPConnection::connect() {
        push_segments_out(true);
    }

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            unclean_shutdown(true);
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

bool TCPConnection::push_segments_out(bool send_syn) {
    _sender.fill_window(send_syn || in_syn_recv());
    TCPSegment seg;
    while (!_sender.segments_out().empty()) {
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        if (_need_send_rst) {
            _need_send_rst = false;
            seg.header().rst = true;
        }
        segments_out().push(seg);
    }
    clean_shutdown();
    return true;
}
void TCPConnection::unclean_shutdown(bool send_rst) {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
    if (send_rst) {
        _need_send_rst = true;
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        push_segments_out();
    }
}
bool TCPConnection::clean_shutdown() {
    if (!_sender.stream_in().eof() && _receiver.stream_out().input_ended()) {
        _linger_after_streams_finish = false;
    }
    if (_sender.stream_in().eof() && _receiver.stream_out().input_ended() && _sender.bytes_in_flight() == 0) {
        if (!_linger_after_streams_finish || _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
    return !_active;
}

bool TCPConnection::in_syn_recv() {
    return _receiver.ackno().has_value() && !_receiver.stream_out().input_ended(); 
}

bool TCPConnection::in_syn_sent() {
    return _sender.next_seqno_absolute() > 0 && _sender.next_seqno_absolute() == bytes_in_flight(); 
}
