#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const auto &hdr = seg.header();
    const auto &data = seg.payload();
    // Not receive SYN yet
    if (!_syn) {
        if (!hdr.syn) {
            return;
        } else {
            _syn = true;
            // the sequence number of the first segment is the initial sequence number
            _isn = hdr.seqno;
        }
    }
    // NOW, received SYN from the other side
    if (hdr.fin) {
        _fin = true;
    }
    // the checkpoint should be the last absolute sequence number
    size_t ckpt = stream_out().bytes_written() + 1;
    uint64_t absolute_seqno = unwrap(hdr.seqno, _isn, ckpt);
    // In the first segment, the stream index should be 0, or this index should be absolute seqno minus 1
    uint64_t stream_idx = absolute_seqno + static_cast<uint64_t>(hdr.syn) - 1;
    _reassembler.push_substring(data.copy(), stream_idx, _fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn) {
        return nullopt;
    }
    // written bytes + SYN
    uint64_t absolute_ackno = stream_out().bytes_written() + 1;
    // Only when there is no segment on the fly and receive the FIN flag, then the absolute seq add 1
    absolute_ackno += (_fin && _reassembler.unassembled_bytes() == 0) ? 1 : 0;
    return wrap(absolute_ackno, _isn);
}

size_t TCPReceiver::window_size() const {
    // the capacity minus the bytes have been reassembled, but not consumed
    return this->_capacity - this->stream_out().buffer_size();
}
