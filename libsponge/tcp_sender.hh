#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <vector>
#include <queue>

class RetransTimer {
  private:
    uint32_t _remaining_time{0};
    bool _is_stopped{false};
    bool _is_expired{false};

  public:
    RetransTimer() : _remaining_time(0), _is_stopped(true), _is_expired(false) {}
    RetransTimer(uint32_t init_time, bool stopped, bool expired)
        : _remaining_time(init_time), _is_stopped(stopped), _is_expired(expired) {}
    void tick_to_retrans_timer(uint32_t ms_since_last_tick) {
        if (_is_stopped) {
            return;
        }
        if (ms_since_last_tick >= _remaining_time) {
            _remaining_time = 0;
            _is_expired = true;
        } else {
            _remaining_time -= ms_since_last_tick;
        }
    }
    bool is_expired() const { return _is_expired; }
    bool is_stopped() const { return _is_stopped; }
    void start_new_timer(uint32_t new_rto) {
        _remaining_time = new_rto;
        _is_expired = false;
        _is_stopped = false;
    }

    void stop_retrans_timer() {
        _is_stopped = true;
        _is_expired = false;
        _remaining_time = 0;
    }
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! Some TCP state flags send to the other side,
    bool _syn_sent{false};
    //! When FIN is sent, it means that the data stream is closed on its own side, `fill_window` will return directly
    bool _fin_sent{false};
    //! the receive windows size, from the other side
    uint16_t _win_size{1};

    //! Once the segment is filled the window(using the data payload), it will be sent to the other side
    //! In this lab `send_segments` means move the segment to `_segments_out` FIFO and `_outstanding_segments` map
    void send_segment(TCPSegment &seg);
    //! keep track of segments which have been sent but not yet acked by the receiver
    //!@{
    // first-> the absolute sequence number, it will be mono increased
    // second-> the outstanding tcp segment
    std::vector<std::pair<size_t, TCPSegment>> _outstanding_segments{};
    size_t _outstanding_bytes{0};
    // !@}

    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;
    RetransTimer _retrans_timer{};
    //! current retransmission timeout value, aka RTO
    uint64_t _current_retransmission_timeout{0};
    unsigned int _consecutive_retransmission_cnt{0};

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
