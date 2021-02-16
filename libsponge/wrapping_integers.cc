#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t b = n & 0xffffffff;
    return WrappingInt32{isn.raw_value()+b};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t offset = n.raw_value() - isn.raw_value(); 
    uint64_t highBits = (checkpoint>>32)<<32;
    uint64_t a = highBits + offset;
    uint64_t b = a + (1ul<<32);
    uint64_t c = a > 1ul<<32 ? a - (1ul<<32) : b;
    int64_t delta1 = abs(static_cast<int64_t>(a-checkpoint));
    int64_t delta2 = abs(static_cast<int64_t>(b-checkpoint));
    int64_t delta3 = abs(static_cast<int64_t>(c-checkpoint));
    int64_t m = min(min(delta1,delta2),delta3);
    if (m == delta1) return a;
    else if (m == delta2) return b;
    else return c;
}
