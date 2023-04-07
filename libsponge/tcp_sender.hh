#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

// RTO Timer
class TCPTimer {
  private:
    size_t _tick_passed = 0;      // 记录实时的时间戳
    size_t _rto_timeout = 0;      // 记录超过多久时间没有收到ACK就重传
    unsigned int _rto_count = 0;  // 记录重传的次数

    bool _is_running{false};  // 记录计时器是否启动

  public:
    // 重置计时器
    void reset(const uint16_t retx_timeout) {
        _rto_count = 0;
        _rto_timeout = retx_timeout;
        _tick_passed = 0;
    }

    // 启动计时器
    void run() { _is_running = true; }

    // 暂停计时器
    void stop() { _is_running = false; }

    // 计时器是否启动
    bool is_running() const { return _is_running; }

    // 重传次数
    unsigned int rto_count() const { return _rto_count; }

    // 慢启动
    void slow_start() {
        _rto_count++;
        _rto_timeout *= 2;
    }

    // 更新当前时间
    void update(const size_t ms_since_last_tick) { _tick_passed += ms_since_last_tick; }

    // 检测是否超时
    bool is_timeout() const { return _is_running && _tick_passed >= _rto_timeout; }

    // 重新计时
    void restart() { _tick_passed = 0; }
};

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    TCPTimer _rto_timer{};

    // 记录确认的_ackno
    size_t _ackno = 0;

    // 记录窗口大小，并标记是否为空窗口
    size_t _window_size = 1;

    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    // 缓存队列
    std::queue<TCPSegment> _cache{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    // 发送数据段
    void _send_segment(const TCPSegment &seg);

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
