#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.


using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_seg_received_ms; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _debugger.print_segment(*this, seg, "Segment received!");
    _time_since_last_seg_received_ms = 0;

    // Receive a RST segment
    if (seg.header().rst) {
        set_rst_state();
        return;
    }
    _receiver.segment_received(seg);
    size_t seg_length = seg.length_in_sequence_space();
    // Passive open a TCP connection, when received the first SYN segment
    if (_receiver.ackno().has_value() && !_receiver.stream_out().input_ended() // SYN_RECV
        && (_sender.next_seqno_absolute() == 0)) { // CLOSED
        // Send a SYN_ACK 
        _sender.fill_window();
        send_segments_in_sender_queue();
        return;
    }

    bool need_send_empty_ack = false;
    //  Simultaneously Open
    if (seg.header().syn && _sender.next_seqno_absolute() > 0) {
        need_send_empty_ack = true;
    }
    // Received segment contains the ACK flag
    if (seg.header().ack) {
        // sender will use the newest ackno&win_size, then it will fill the window.
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg_length != 0 && _sender.segments_out().empty()) {
            need_send_empty_ack = true;
        }
    }
    // Received a keep-alive segment
    bool need_response_keep_alive =
        (_receiver.ackno().has_value() && seg_length == 0 && seg.header().seqno == _receiver.ackno().value() - 1);
    if (need_send_empty_ack || need_response_keep_alive) {
        _sender.send_empty_segment();
    }
    // Passive close , it means the inbound stream ends before the outbound stream reached EOF
    // no need to TIME_WAIT, turns to CLOSE_WAIT state
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }
    // Passive close side from LAST_ACK state turns to CLOSED state
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        _active = false;
    }

    send_segments_in_sender_queue();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t written_size = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments_in_sender_queue();
    return written_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        set_rst_state();
        _sender.segments_out() = {};
        send_rst_segment();
        return;
    }
    send_segments_in_sender_queue();
    _time_since_last_seg_received_ms += ms_since_last_tick;
    // End the connection cleanly if expired the TIE_WAIT timeout
    // TCP connection turns to CLOSED state
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish &&
        _time_since_last_seg_received_ms >= 10 * _cfg.rt_timeout) {
        _active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    //! when the output stream is shutdown and empty, the sender will send FIN flag
    _sender.fill_window();
    send_segments_in_sender_queue();
}

void TCPConnection::connect() {
    _sender.fill_window();
    // Active open a TCP connection
    _active = true;
    send_segments_in_sender_queue();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            set_rst_state();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::set_rst_state() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
    _linger_after_streams_finish = false;
}

void TCPConnection::send_rst_segment() {
    TCPSegment segment;
    segment.header().seqno = _sender.next_seqno();
    segment.header().rst = true;
    _segments_out.push(segment);
}

void TCPConnection::send_segments_in_sender_queue() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        // Only when it is sending the first SYN segment, the `ackno` is `nullopt`
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = _receiver.window_size();
        _segments_out.push(seg);
        _debugger.print_segment(*this, seg, "Segment sent!");
    }
}
