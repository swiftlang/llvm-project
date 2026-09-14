//===- SocketSimpleRemoteCA.h - SimpleRemote CA over a socket ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// SimpleRemote protocol over a connected socket.
//
// POSIX only: the reactor is poll(2) over the connection and a wake socket.
//
//===----------------------------------------------------------------------===//

#ifndef ORC_RT_BEDROCK_SPS_SOCKETSIMPLEREMOTECA_H
#define ORC_RT_BEDROCK_SPS_SOCKETSIMPLEREMOTECA_H

#include "orc-rt/bedrock/SocketHandle.h"
#include "orc-rt/bedrock/sps/SimpleRemoteCA.h"
#include "orc-rt/support/ExecutorAddress.h"
#include "orc-rt/support/span.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>

namespace orc_rt {

/// Carries SimpleRemote messages over a connected socket.
///
/// Owns a reactor thread that handles message IO and dispatch. The wire format
/// matches LLVM's SimpleRemoteEPC.
class SocketSimpleRemoteCA : public SimpleRemoteCA {
  friend class SocketSimpleRemoteCATest;

public:
  /// Takes ownership of Sock, which must be a connected stream socket.
  static Expected<std::shared_ptr<SocketSimpleRemoteCA>>
  Create(Session &S, SocketHandle Sock);

private:
  enum class State {
    NotConnected, ///< Before connect. Nothing may be registered or queued.
    Running,      ///< Calls may be registered, messages queued.
    Draining,     ///< Hang-up queued and latched: nothing may follow it.
    Closed,       ///< Reactor stopped and descriptors released.
  };

  /// The on-wire message header: four little-endian uint64 fields, followed by
  /// the payload.
  ///
  /// The sole authority for the layout, so that the encoder and the reader
  /// cannot disagree about it.
  struct MsgHeader {
    static constexpr size_t Size = 4 * sizeof(uint64_t);

    /// A decoded header. MsgSize counts the header as well as the payload.
    struct Fields {
      uint64_t MsgSize = 0;
      uint64_t OpC = 0;
      uint64_t SeqNo = 0;
      ExecutorAddr Tag;
    };

    static void encode(char *Buf, Opcode Op, uint64_t SeqNo,
                       orc_rt_ControllerHandlerTag T, size_t PayloadSize);
    static Fields decode(const char *Buf);
  };

  /// A framed message waiting to go out. Owns the payload. Supports interrupted
  /// sends via pending/advance/complete.
  class OutgoingMessage {
  public:
    OutgoingMessage(Opcode Op, uint64_t SeqNo, orc_rt_ControllerHandlerTag T,
                    WrapperFunctionBuffer Payload);

    /// The next bytes to send. Empty once the message has gone.
    span<const char> pending() const;

    void advance(size_t N) { Sent += N; }
    bool complete() const { return Sent == MsgHeader::Size + Payload.size(); }

  private:
    char Header[MsgHeader::Size];
    WrapperFunctionBuffer Payload;
    size_t Sent = 0;
  };

  /// Incoming message. Supports interrupted reads via pending/advance/complete.
  /// The header is filled first then decoded to determine the allocation size
  /// for the payload buffer.
  class IncomingMessage {
  public:
    /// Where the next bytes read should land. Empty once the current buffer is
    /// full: the header is then ready to decode, or the message is complete.
    span<char> pending();

    void advance(size_t N) { Filled += N; }

    /// True once the payload buffer is full and the message can be dispatched.
    bool complete() const {
      return Decoded && Filled == MsgHeader::Size + Payload.size();
    }

    /// Decodes the completed header and allocates space for the payload.
    /// Fails on a header that describes an impossible message.
    Error decodeHeader();

    const MsgHeader::Fields &fields() const { return F; }

    /// Take the payload. Resets this value for the next message.
    WrapperFunctionBuffer take();

  private:
    char Header[MsgHeader::Size];
    WrapperFunctionBuffer Payload;
    size_t Filled = 0;
    bool Decoded = false;
    MsgHeader::Fields F;
  };

  SocketSimpleRemoteCA(Session &S, SocketHandle Sock, SocketHandle WakeRead,
                       SocketHandle WakeWrite)
      : SimpleRemoteCA(S), Sock(std::move(Sock)), WakeRead(std::move(WakeRead)),
        WakeWrite(std::move(WakeWrite)) {}

  // Session::ControllerAccess.
  void connect(BootstrapInfo BI) override;
  void disconnect() override;
  void callController(OnControllerCallReturn OnComplete,
                      orc_rt_ControllerHandlerTag T,
                      WrapperFunctionBuffer ArgBytes) override;
  void sendWrapperResult(WrapperFunctionBuffer ResultBytes,
                         uint64_t CallId) override;

  /// Wake the reactor thread.
  ///
  /// Makes a poll in progress return, or the next one return at once.
  ///
  /// Caller must hold M and must not be in the Closed state: the
  /// reactor releases the descriptors under the same lock, so holding
  /// it is what keeps this from writing to a closed one.
  void wakeReactorLocked();

  /// Reactor thread entry point. Runs the reactor loop, then performs cleanup:
  /// releasing descriptors, failing outstanding calls, and notifying the
  /// session.
  void runReactor();

  /// Message IO loop: polls, reads, dispatches and sends until the connection
  /// ends. Returns the reason it stopped: success if either side hung up.
  Error reactorLoop();

  /// Sends as much of the queue as the socket will take. Reactor thread only.
  Error drainSends();

  /// Receives what is available and hands each complete message to
  /// handleMessage. Incoming carries any part-assembled message across calls.
  /// Reactor thread only.
  ///
  /// Only a hang-up ends the session cleanly. Unexpected end of stream is an
  /// error.
  Expected<Action> readAndDispatch(IncomingMessage &Incoming);

  /// Takes the handler for SeqNo under M.
  OnControllerCallReturn takePendingCall(uint64_t SeqNo) override;

  SocketHandle Sock;
  SocketHandle WakeRead, WakeWrite;

  std::mutex M;
  State CurState = State::NotConnected;

  /// Framed messages awaiting send. Only the reactor pops, so it may hold a
  /// reference to the front across a send without the lock.
  ///
  /// TODO: Currently unbounded. We may want to add a bound on this.
  std::deque<OutgoingMessage> Queue;
};

} // namespace orc_rt

#endif // ORC_RT_BEDROCK_SPS_SOCKETSIMPLEREMOTECA_H
