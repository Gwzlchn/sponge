#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {
    _current_retransmission_timeout = _initial_retransmission_timeout;
}

uint64_t TCPSender::bytes_in_flight() const { return _outstanding_bytes; }

//! \details set the segment header and payload, fill the other side receive window size as much as possible
void TCPSender::fill_window() {
    size_t receiver_win_size = _win_size ? _win_size : 1;
    while ((receiver_win_size > _outstanding_bytes) && !_fin_sent) {
        TCPSegment seg;
        if (!_syn_sent) {
            seg.header().syn = true;
            _syn_sent = true;
        }
        seg.header().seqno = next_seqno();

        // the max bytes could this segment carried
        size_t max_payload_size =
            min(TCPConfig::MAX_PAYLOAD_SIZE, receiver_win_size - _outstanding_bytes - seg.header().syn);
        string payload = _stream.read(max_payload_size);
        seg.payload() = Buffer(std::move(payload));
        size_t seg_length = seg.length_in_sequence_space();
        // send FIN flag if reached EOF of stream
        if (!_fin_sent && _stream.eof() && seg_length + _outstanding_bytes < receiver_win_size) {
            seg.header().fin = true;
            _fin_sent = true;
            seg_length++;
        }
        if (seg_length) {
            send_segment(seg);
        } else {
            break;
        }
    }
}
//! The segment here is NOT EMPTY (non zero length in sequence space)
void TCPSender::send_segment(TCPSegment &seg) {
    _segments_out.push(seg);
    _outstanding_segments.emplace_back(_next_seqno, seg);
    const auto seg_length = seg.length_in_sequence_space();
    _next_seqno += seg_length;
    _outstanding_bytes += seg_length;

    if (_retrans_timer.is_stopped()) {
        _retrans_timer.start_new_timer(_current_retransmission_timeout);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //! `absolute_ackno` is the number of bytes that the receiver received.
    //! `_next_seqno` is the number of bytes that the sender wants to send, i.e. the last `absolute-seqno`
    size_t absolute_ackno = unwrap(ackno, _isn, _next_seqno);
    if (absolute_ackno > _next_seqno) {
        return;
    }
    _win_size = window_size;
    //! Remove segments that have now been fully acknoledged segment in `_outstanding_segment`
    auto iter = _outstanding_segments.begin();
    bool acked_new_data = false;
    while (!_outstanding_segments.empty()) {
        const auto &seg = iter->second;
        const auto seg_length = seg.length_in_sequence_space();
        if (iter->first + seg_length <= absolute_ackno) {
            // erase returns the iterator following the last removed element.
            iter = _outstanding_segments.erase(iter);
            _outstanding_bytes -= seg_length;
            acked_new_data = true;
        } else {
            break;
        }
    }
    //! When received a valid ackno, which means the receiver receipt of the new data
    //! the retransmission timer will restart if there are outstanding segments (for the current value of RTO).,
    //! otherwise the timer will stop
    if (acked_new_data) {
        _current_retransmission_timeout = _initial_retransmission_timeout;
        if (!_outstanding_segments.empty()) {
            _retrans_timer.start_new_timer(_current_retransmission_timeout);
        } else {
            _retrans_timer.stop_retrans_timer();
        }
        _consecutive_retransmission_cnt = 0;
    }
    fill_window();
    return;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _retrans_timer.tick_to_retrans_timer(ms_since_last_tick);
    // If the retrans_timer is expired, it will retransmit the earliest segment when the window size is not zero
    // then double the RTO, restart a new timer.
    if (_retrans_timer.is_expired() && !_outstanding_segments.empty()) {
        auto iter = _outstanding_segments.begin();
        if (_win_size > 0) {
            _current_retransmission_timeout <<= 1;
            _consecutive_retransmission_cnt++;
        }
        if (_consecutive_retransmission_cnt <= TCPConfig::MAX_RETX_ATTEMPTS) {
          _segments_out.push(iter->second);
        }
        _retrans_timer.start_new_timer(_current_retransmission_timeout);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission_cnt; }

//! \details The segment with zero data and correct `seqno` is useful for `ACK` the other side.
// it will never be retransmitted, and doesn't need to keep track.
void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
    return;
}
