#ifndef SPONGE_LIBSPONGE_TCP_FACTORED_HH
#define SPONGE_LIBSPONGE_TCP_FACTORED_HH

#include "tcp_config.hh"
#include "tcp_receiver.hh"
#include "tcp_sender.hh"
#include "tcp_state.hh"
#ifdef DEBUG
#include <iostream>
#endif
template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \brief A complete endpoint of a TCP connection
class TCPConnection {
    class TCPConnectionDebugger {
      private:
        bool open_debugger{true};

      public:
        TCPConnectionDebugger() : open_debugger(true) {}
        ~TCPConnectionDebugger() {}

        std::string color_1(const std::string &data) { return data; }

        std::string color_2(const std::string &data) { return data; }

        void print_segment(const TCPConnection &that,
                           const TCPSegment &seg,
                           const std::string &desription,
                           bool check = true) {
            DUMMY_CODE(that, seg, desription, check);
#ifdef DEBUG
            std::cerr << "\n" << color_1(desription) << "\n";
            std::cerr << (color_2("Flag") + " : ") << (seg.header().syn ? "S" : "") << (seg.header().fin ? "F" : "")
                      << (seg.header().ack ? "A" : "") << (seg.header().rst ? "R" : "") << "\n"
                      << (color_2("Sequnce Number") + " : ") << (seg.header().seqno.raw_value()) << "\n"
                      << (color_2("Acknowledgement Number") + " : ") << (seg.header().ackno) << "\n"
                      << (color_2("Window Size") + " : ") << (seg.header().win) << "\n"
                      << (color_2("Payload") + " : ") << (seg.payload().size() ? seg.payload().str() : "empty string")
                      << "\n"
                      << (color_2("Payload Size") + " : ") << (seg.payload().size()) << "\n"
                      << (color_2("Sequnce Space") + " : ") << (seg.length_in_sequence_space()) << "\n"
                      << (color_2("ackno of sender") + " : ") << (that._sender.next_seqno_absolute()) << "\n"
                      << (color_2("next seqno absolute of sender") + " : ") << (that._sender.next_seqno_absolute())
                      << "\n";
            std::cerr << (color_2("Active: ") + ((that._active) ? "Y " : "N"))
                      << (color_2("  TIME_WAIT: ") + ((that._linger_after_streams_finish) ? "Y\n" : "N\n"));
            std::cerr << (color_2("Sender State: ")) << that.state().name() << std::endl;
#endif
        }
    };

  private:
    TCPConnectionDebugger _debugger{};
    TCPConfig _cfg;
    TCPReceiver _receiver{_cfg.recv_capacity};
    TCPSender _sender{_cfg.send_capacity, _cfg.rt_timeout, _cfg.fixed_isn};

    //! outbound queue of segments that the TCPConnection wants sent
    std::queue<TCPSegment> _segments_out{};

    //! Should the TCPConnection stay active (and keep ACKing)
    //! for 10 * _cfg.rt_timeout milliseconds after both streams have ended,
    //! in case the remote TCPConnection doesn't know we've received its whole stream?
    bool _linger_after_streams_finish{true};

    //! Send all segment in sender's output queue, and set the ACK flag and a proper `ackno`
    void send_segments_in_sender_queue();
    //! TCP Conn inner state
    //! TCP connection in active state means that it is other than CLOSED state
    //! the init state is LISTENING
    bool _active{true};

    //! Once TCP connection receive a new segment, it will be reset to 0.
    //! Otherwise, It is a monotonically increasing value in `tick` function
    size_t _time_since_last_seg_received_ms{0};

    //! Send a RST packet, or receive a RST packet
    void send_rst_segment();
    //! unclear shutdown current TCP Connection immediately
    void set_rst_state();

  public:
    //! \name "Input" interface for the writer
    //!@{

    //! \brief Initiate a connection by sending a SYN segment
    void connect();

    //! \brief Write data to the outbound byte stream, and send it over TCP if possible
    //! \returns the number of bytes from `data` that were actually written.
    size_t write(const std::string &data);

    //! \returns the number of `bytes` that can be written right now.
    size_t remaining_outbound_capacity() const;

    //! \brief Shut down the outbound byte stream (still allows reading incoming data)
    void end_input_stream();
    //!@}

    //! \name "Output" interface for the reader
    //!@{

    //! \brief The inbound byte stream received from the peer
    ByteStream &inbound_stream() { return _receiver.stream_out(); }
    //!@}

    //! \name Accessors used for testing

    //!@{
    //! \brief number of bytes sent and not yet acknowledged, counting SYN/FIN each as one byte
    size_t bytes_in_flight() const;
    //! \brief number of bytes not yet reassembled
    size_t unassembled_bytes() const;
    //! \brief Number of milliseconds since the last segment was received
    size_t time_since_last_segment_received() const;
    //!< \brief summarize the state of the sender, receiver, and the connection
    TCPState state() const { return {_sender, _receiver, active(), _linger_after_streams_finish}; };
    //!@}

    //! \name Methods for the owner or operating system to call
    //!@{

    //! Called when a new segment has been received from the network
    void segment_received(const TCPSegment &seg);

    //! Called periodically when time elapses
    void tick(const size_t ms_since_last_tick);

    //! \brief TCPSegments that the TCPConnection has enqueued for transmission.
    //! \note The owner or operating system will dequeue these and
    //! put each one into the payload of a lower-layer datagram (usually Internet datagrams (IP),
    //! but could also be user datagrams (UDP) or any other kind).
    std::queue<TCPSegment> &segments_out() { return _segments_out; }

    //! \brief Is the connection still alive in any way?
    //! \returns `true` if either stream is still running or if the TCPConnection is lingering
    //! after both streams have finished (e.g. to ACK retransmissions from the peer)
    bool active() const;
    //!@}

    //! Construct a new connection from a configuration
    explicit TCPConnection(const TCPConfig &cfg) : _cfg{cfg} {}

    //! \name construction and destruction
    //! moving is allowed; copying is disallowed; default construction not possible

    //!@{
    ~TCPConnection();  //!< destructor sends a RST if the connection is still open
    TCPConnection() = delete;
    TCPConnection(TCPConnection &&other) = default;
    TCPConnection &operator=(TCPConnection &&other) = default;
    TCPConnection(const TCPConnection &other) = delete;
    TCPConnection &operator=(const TCPConnection &other) = delete;
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_FACTORED_HH
