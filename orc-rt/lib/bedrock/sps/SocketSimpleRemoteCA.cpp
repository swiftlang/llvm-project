//===- SocketSimpleRemoteCA.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// SimpleRemote protocol over a connected socket, on POSIX.
//
//===----------------------------------------------------------------------===//

#include "orc-rt/bedrock/sps/SocketSimpleRemoteCA.h"

#include "orc-rt-c/support/Logging.h"
#include "orc-rt-internal/support/Endian.h"
#include "orc-rt-internal/support/sys/Errno.h"

#include "orc-rt/support/Compiler.h"

#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace orc_rt {

namespace {

// Header field offsets. Wire values, shared with LLVM's FDMsgHeader: do not
// reorder.
constexpr size_t MsgSizeOffset = 0;
constexpr size_t OpCOffset = 8;
constexpr size_t SeqNoOffset = 16;
constexpr size_t TagAddrOffset = 24;

Error makeError(const char *Op, int ErrNum) {
  return make_error<StringError>(std::string(Op) +
                                 " failed: " + sys::strError(ErrNum));
}

bool isWouldBlock(int ErrNum) {
  return ErrNum == EAGAIN || ErrNum == EWOULDBLOCK;
}

/// O_NONBLOCK rather than MSG_DONTWAIT on each send/recv: Darwin defines
/// MSG_DONTWAIT but does not honour it on sends.
Error setNonBlocking(int FD) {
  int Flags = fcntl(FD, F_GETFL, 0);
  if (Flags == -1)
    return makeError("fcntl(F_GETFL)", errno);
  if ((Flags & O_NONBLOCK) == 0 && fcntl(FD, F_SETFL, Flags | O_NONBLOCK) == -1)
    return makeError("fcntl(F_SETFL)", errno);
  return Error::success();
}

template <typename OpT> ssize_t retryOnEINTR(OpT Op) {
  for (;;) {
    ssize_t N = Op();
    if (N >= 0 || errno != EINTR)
      return N;
  }
}

} // namespace

void SocketSimpleRemoteCA::MsgHeader::encode(char *Buf, Opcode Op,
                                             uint64_t SeqNo,
                                             orc_rt_ControllerHandlerTag T,
                                             size_t PayloadSize) {
  endian_write<uint64_t>(Buf + MsgSizeOffset, Size + PayloadSize,
                         endian::little);
  endian_write<uint64_t>(Buf + OpCOffset, static_cast<uint64_t>(Op),
                         endian::little);
  endian_write<uint64_t>(Buf + SeqNoOffset, SeqNo, endian::little);
  endian_write<uint64_t>(Buf + TagAddrOffset, reinterpret_cast<uintptr_t>(T),
                         endian::little);
}

SocketSimpleRemoteCA::MsgHeader::Fields
SocketSimpleRemoteCA::MsgHeader::decode(const char *Buf) {
  Fields F;
  F.MsgSize = endian_read<uint64_t>(Buf + MsgSizeOffset, endian::little);
  F.OpC = endian_read<uint64_t>(Buf + OpCOffset, endian::little);
  F.SeqNo = endian_read<uint64_t>(Buf + SeqNoOffset, endian::little);
  F.Tag =
      ExecutorAddr(endian_read<uint64_t>(Buf + TagAddrOffset, endian::little));
  return F;
}

SocketSimpleRemoteCA::OutgoingMessage::OutgoingMessage(
    Opcode Op, uint64_t SeqNo, orc_rt_ControllerHandlerTag T,
    WrapperFunctionBuffer Payload)
    : Payload(std::move(Payload)) {
  MsgHeader::encode(Header, Op, SeqNo, T, this->Payload.size());
}

span<const char> SocketSimpleRemoteCA::OutgoingMessage::pending() const {
  if (Sent < MsgHeader::Size)
    return {Header + Sent, MsgHeader::Size - Sent};
  size_t InPayload = Sent - MsgHeader::Size;
  return {Payload.data() + InPayload, Payload.size() - InPayload};
}

span<char> SocketSimpleRemoteCA::IncomingMessage::pending() {
  if (!Decoded)
    return {Header + Filled, MsgHeader::Size - Filled};
  size_t InPayload = Filled - MsgHeader::Size;
  return {Payload.data() + InPayload, Payload.size() - InPayload};
}

Error SocketSimpleRemoteCA::IncomingMessage::decodeHeader() {
  F = MsgHeader::decode(Header);
  if (F.MsgSize < MsgHeader::Size)
    return make_error<StringError>("Message size smaller than its header");

  Payload = WrapperFunctionBuffer::allocate(F.MsgSize - MsgHeader::Size);
  Decoded = true;
  return Error::success();
}

WrapperFunctionBuffer SocketSimpleRemoteCA::IncomingMessage::take() {
  auto P = std::move(Payload);
  Payload = WrapperFunctionBuffer();
  Filled = 0;
  Decoded = false;
  return P;
}

Expected<std::shared_ptr<SocketSimpleRemoteCA>>
SocketSimpleRemoteCA::Create(Session &S, SocketHandle Sock) {
  // Sock is owned here, so every early return below closes it.
  if (auto Err = setNonBlocking(Sock.get()))
    return std::move(Err);

  int Pair[2];
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, Pair) != 0)
    return makeError("socketpair", errno);
  SocketHandle WakeRead(Pair[0]), WakeWrite(Pair[1]);

  // Both ends non-blocking. A blocking write end would stall a sender inside M,
  // which the reactor needs before it can drain -- a deadlock; a blocking read
  // end would leave the drain loop waiting for a byte after emptying the queue.
  for (int W : {WakeRead.get(), WakeWrite.get()})
    if (auto Err = setNonBlocking(W))
      return std::move(Err);

  // Not make_shared: the constructor is private.
  return std::shared_ptr<SocketSimpleRemoteCA>(new SocketSimpleRemoteCA(
      S, std::move(Sock), std::move(WakeRead), std::move(WakeWrite)));
}

void SocketSimpleRemoteCA::wakeReactorLocked() {
  assert(CurState != State::Closed && "wake on a released descriptor");
  char C = 0;
  ssize_t N = retryOnEINTR(
      [&] { return ::send(WakeWrite.get(), &C, 1, MSG_NOSIGNAL); });
  int ErrNum = errno;
  if (N < 0 && !isWouldBlock(ErrNum)) {
    // send to wait socket failed. Log the reason in case this jams up the
    // reactor.
    ORC_RT_LOG(Info, ControllerAccess,
               "SocketSimpleRemoteCA wake-send error: " ORC_RT_LOG_PUB_S,
               sys::strError(ErrNum).c_str());
  }
}

void SocketSimpleRemoteCA::connect(BootstrapInfo BI) {
  {
    std::scoped_lock<std::mutex> Lock(M);
    assert(CurState == State::NotConnected && "connect called twice");
    CurState = State::Running;
    // Queued before the reactor exists, so setup is necessarily first out and
    // the reactor never reads on a connection that is not yet accepting.
    Queue.emplace_back(Opcode::Setup, 0, nullptr, encodeSetup(BI));
  }

  // The Session holds this alive until notifyDisconnected, which the reactor
  // reaches only as it exits, so the thread needs no reference of its own.
  //
  // FIXME: Report a spawn failure, and offer a pumped mode that borrows the
  // caller's thread. std::thread's constructor aborts rather than reporting
  // under -fno-exceptions, so connect cannot surface one today.
  std::thread([this] { runReactor(); }).detach();
}

void SocketSimpleRemoteCA::disconnect() {
  std::scoped_lock<std::mutex> Lock(M);
  // Anything but Running means teardown is under way or done, or the connection
  // never opened. The Session tolerates a disconnect racing a remote one.
  if (CurState != State::Running)
    return;

  // Queued and latched in one lock hold. Draining is what keeps anything from
  // landing behind the hang-up, so it must take effect with the same atomicity
  // as the queueing.
  Queue.emplace_back(Opcode::Hangup, 0, nullptr,
                     encodeHangup(Error::success()));
  CurState = State::Draining;
  wakeReactorLocked();
}

void SocketSimpleRemoteCA::callController(OnControllerCallReturn OnComplete,
                                          orc_rt_ControllerHandlerTag T,
                                          WrapperFunctionBuffer ArgBytes) {
  {
    std::scoped_lock<std::mutex> Lock(M);
    if (CurState == State::Running) {
      // Registered and queued in one lock hold, so a call is never left pending
      // with nothing to answer it, nor sent with no handler to complete.
      uint64_t SeqNo = registerCall(std::move(OnComplete));
      Queue.emplace_back(Opcode::Call, SeqNo, T, std::move(ArgBytes));
      wakeReactorLocked();
      return;
    }
  }

  // The connection is gone, so no result can arrive. The caller is still on the
  // stack, so fail the handler there.
  failControllerCallInline(std::move(OnComplete));
}

void SocketSimpleRemoteCA::sendWrapperResult(WrapperFunctionBuffer ResultBytes,
                                             uint64_t CallId) {
  // No state check of its own: a result has no pending call on this side, so a
  // departed connection drops it with nothing left unsettled.
  std::scoped_lock<std::mutex> Lock(M);
  if (CurState != State::Running)
    return;
  Queue.emplace_back(Opcode::Result, CallId, nullptr, std::move(ResultBytes));
  wakeReactorLocked();
}

void SocketSimpleRemoteCA::runReactor() {
  Error Err = reactorLoop();

  {
    std::scoped_lock<std::mutex> Lock(M);
    // Closed refuses every further send and wake.
    CurState = State::Closed;
    Sock.reset();
    WakeRead.reset();
    WakeWrite.reset();
  }

  // Before the notification, while the managed-code group is still open, or the
  // handlers are dropped rather than dispatched.
  PendingCallsMap Failed;
  {
    std::scoped_lock<std::mutex> Lock(M);
    Failed = takeAllCalls();
  }
  for (auto &[SeqNo, OnComplete] : Failed)
    failPendingControllerCall(std::move(OnComplete));

  // Last thing to touch this: it may run the destructor.
  notifyDisconnected(std::move(Err));
}

Error SocketSimpleRemoteCA::reactorLoop() {
  // Part-assembled incoming message.
  IncomingMessage Incoming;

  for (;;) {
    pollfd PollFDs[2];
    PollFDs[0].fd = Sock.get();
    PollFDs[0].events = POLLIN;
    PollFDs[0].revents = 0;
    PollFDs[1].fd = WakeRead.get();
    PollFDs[1].events = POLLIN;
    PollFDs[1].revents = 0;

    // Check for outgoing messages / draining-state.
    {
      std::scoped_lock<std::mutex> Lock(M);
      if (!Queue.empty())
        PollFDs[0].events |= POLLOUT;
      else if (CurState == State::Draining)
        return Error::success(); // The hang-up has gone out.
    }

    // Check readiness.
    while (::poll(PollFDs, 2, /*timeout=*/-1) < 0) {
      if (errno == EINTR)
        continue;
      return makeError("poll", errno);
    }

    // Drain the wake notification so that next poll blocks.
    if (PollFDs[1].revents & POLLIN) {
      char Buf[64];
      while (::recv(WakeRead.get(), Buf, sizeof(Buf), 0) > 0)
        ;
    }

    // Read before sending, so that a peer's parting hang-up reaches us rather
    // than the EPIPE from a send to a peer that has already gone.
    //
    // POLLHUP and POLLERR read too. A closed peer may have left buffered
    // messages, and recv returning zero is what separates an orderly close from
    // a truncated one; POLLERR says only that something is pending, so recv is
    // what reports what.
    auto Next = Action::Continue;
    if (PollFDs[0].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
      auto A = readAndDispatch(Incoming);
      if (!A)
        return A.takeError();
      Next = *A;
    }

    // If readAndDispatch got a hang-up then just do a best-effort send of the
    // remaining queue items, then return.
    if (Next == Action::End) {
      if (auto Err = drainSends()) {
        [[maybe_unused]] std::string Msg = toString(std::move(Err));
        ORC_RT_LOG(Info, ControllerAccess,
                   "SocketSimpleRemoteCA final-flush: " ORC_RT_LOG_PUB_S,
                   Msg.c_str());
      }
      return Error::success();
    }

    // Otherwise send any remaining queue items.
    if (PollFDs[0].revents & POLLOUT)
      if (auto Err = drainSends())
        return Err;
  }
}

Error SocketSimpleRemoteCA::drainSends() {
  for (;;) {
    OutgoingMessage *Msg = nullptr;
    {
      std::scoped_lock<std::mutex> Lock(M);
      if (Queue.empty())
        return Error::success();
      Msg = &Queue.front();
    }

    // Sent without the lock: only the reactor pops, and appending to a deque
    // never moves an existing element.
    auto Bytes = Msg->pending();
    ssize_t N = retryOnEINTR([&] {
      return ::send(Sock.get(), Bytes.data(), Bytes.size(), MSG_NOSIGNAL);
    });
    if (N < 0)
      return isWouldBlock(errno) ? Error::success() : makeError("send", errno);
    Msg->advance(N);

    if (Msg->complete()) {
      std::scoped_lock<std::mutex> Lock(M);
      Queue.pop_front();
    }
  }
}

Expected<SocketSimpleRemoteCA::Action>
SocketSimpleRemoteCA::readAndDispatch(IncomingMessage &Incoming) {
  for (;;) {
    // A non-blocking recv can stop anywhere, including mid-header, so this
    // resumes wherever the last left off.
    if (auto Bytes = Incoming.pending(); !Bytes.empty()) {
      ssize_t N = retryOnEINTR(
          [&] { return ::recv(Sock.get(), Bytes.data(), Bytes.size(), 0); });
      if (N < 0) {
        if (isWouldBlock(errno))
          return Action::Continue;
        return makeError("recv", errno);
      }
      if (N == 0)
        return make_error<StringError>(
            "Connection closed without a hang-up message");
      Incoming.advance(N);
      continue;
    }

    // Header full: decode it, then read the payload it describes. An empty
    // payload leaves nothing pending, so the next pass dispatches.
    if (!Incoming.complete()) {
      if (auto Err = Incoming.decodeHeader())
        return Err;
      continue;
    }

    // Taken before dispatching: a handler may send, re-entering this object.
    auto F = Incoming.fields();
    auto A = handleMessage(F.OpC, F.SeqNo, F.Tag, Incoming.take());
    if (!A || *A == Action::End)
      return A;
  }
}

SocketSimpleRemoteCA::OnControllerCallReturn
SocketSimpleRemoteCA::takePendingCall(uint64_t SeqNo) {
  std::scoped_lock<std::mutex> Lock(M);
  return takeCall(SeqNo);
}

} // namespace orc_rt
