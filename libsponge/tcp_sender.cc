#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <random>
#include <string>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window(bool send_syn) {
    // sent a SYN before sent other segment
    if (!_syn_flag) {
        if (send_syn) {
            TCPSegment seg;
            seg.header().syn = true;
            send_segment(seg);
            _syn_flag = true;
        }
        return;
    }

    // take window_size as 1 when it equal 0
 //   if (_window_size == 0) _retransmission_timeout = _initial_retransmission_timeout;  
    size_t win = _window_size > 0 ? _window_size : 1;
    size_t remain;  // window's free space
    // when window isn't full and never sent FIN
    while ((remain = win - (_next_seqno - _recv_ackno)) != 0 && !_fin_flag) {
        size_t size = min(TCPConfig::MAX_PAYLOAD_SIZE, remain);
        TCPSegment seg;
        string str = _stream.read(size);
        seg.payload() = Buffer(std::move(str));
        // add FIN
        if (seg.length_in_sequence_space() < win && _stream.eof()) {
            seg.header().fin = true;
            _fin_flag = true;
        }
        // stream is empty
        if (seg.length_in_sequence_space() == 0) {
            return;
        }
        send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t abs_ackno = unwrap(ackno, _isn, _recv_ackno);
    // out of window, invalid ackno
    if (abs_ackno > _next_seqno) {
        return false;
    }

    // if ackno is legal, modify _window_size before return
    _window_size = window_size;

    // ack has been received
    if (abs_ackno <= _recv_ackno) {
        return true;
    }
    _recv_ackno = abs_ackno;

    // pop all elment before ackno
    while (!_segments_outstanding.empty()) {
        TCPSegment seg = _segments_outstanding.front();
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= abs_ackno) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
        } else {
            break;
        }
    }

    fill_window();
    timer.initialization();
    // _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retransmission = 0;

    // if have other outstanding segment, restart timer
    if (!_segments_outstanding.empty()) {
        timer.start();
    }
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    timer.passed(ms_since_last_tick);
    if (timer.expired() && !_segments_outstanding.empty()) {
        _segments_out.push(_segments_outstanding.front());
        _consecutive_retransmission++;
        if (_window_size > 0 || _recv_ackno == 0) {
            timer.double_rto();
        }
        timer.start();
    }
    if (_segments_outstanding.empty()) {
        timer.end();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission; }

void TCPSender::send_empty_segment() {
    // empty segment doesn't need store to outstanding queue
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

void TCPSender::send_empty_segment(WrappingInt32 seqno) {
    // empty segment doesn't need store to outstanding queue
    TCPSegment seg;
    seg.header().seqno = seqno;
    _segments_out.push(seg);
}

void TCPSender::send_segment(TCPSegment &seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _segments_outstanding.push(seg);
    _segments_out.push(seg);
    if (!timer.is_running()) {  // start timer
        timer.start();
    }
}

