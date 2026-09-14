//===- SocketSimpleRemoteCATest.cpp ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests for SocketSimpleRemoteCA.
//
// Each test attaches a CA to one end of a socket pair and plays the controller
// on the other, so the reactor, the connection state machine and teardown are
// covered without a second process.
//
// The controller side frames messages with the CA's own encoder, reached
// through the friend fixture. That makes these tests about behaviour rather
// than byte layout; conformance to LLVM's SimpleRemoteEPC is established by the
// cross-process regression tests.
//
//===----------------------------------------------------------------------===//

#include "orc-rt/bedrock/sps/SocketSimpleRemoteCA.h"

#include "gtest/gtest.h"

#include "BedrockTestUtils.h"
#include "CommonTestUtils.h"

#include "orc-rt-internal/support/Endian.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <future>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace orc_rt;

namespace orc_rt {

/// Plays the controller against a SocketSimpleRemoteCA.
///
/// A friend of the CA, so that tests frame messages with the CA's own encoder
/// rather than reimplementing it. Only what a test needs is exposed; the public
/// aliases make those names usable from the TEST_F bodies, which are not
/// themselves friends.
class SocketSimpleRemoteCATest : public ::testing::Test {
public:
  using CA = orc_rt::SocketSimpleRemoteCA;
  using MsgHeader = CA::MsgHeader;
  using Opcode = CA::Opcode;

  static constexpr size_t HeaderSize = MsgHeader::Size;

  /// A message as it appears on the wire, with the size field consumed.
  struct Frame {
    MsgHeader::Fields Fields;
    std::vector<char> Payload;

    Opcode opcode() const { return static_cast<Opcode>(Fields.OpC); }
    uint64_t seqNo() const { return Fields.SeqNo; }
    ExecutorAddr tag() const { return Fields.Tag; }

    /// The payload as a view, for comparison against test data.
    std::string_view payload() const {
      return {Payload.data(), Payload.size()};
    }
  };

  /// A connected pair of blocking stream sockets: one end for the CA to adopt,
  /// one for the test to drive.
  static Expected<std::pair<int, int>> makePair() {
    int FDs[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, FDs) != 0)
      return make_error<StringError>(std::string("socketpair: ") +
                                     strerror(errno));
    return std::make_pair(FDs[0], FDs[1]);
  }

  /// The test's end stays blocking, so these just loop until done.
  static Error sendAll(int FDNum, const char *Buf, size_t Size) {
    while (Size) {
      ssize_t N = ::send(FDNum, Buf, Size, MSG_NOSIGNAL);
      if (N < 0) {
        if (errno == EINTR)
          continue;
        return make_error<StringError>(std::string("send: ") + strerror(errno));
      }
      Buf += N;
      Size -= N;
    }
    return Error::success();
  }

  /// Returns short only if the peer closed first.
  static Expected<size_t> recvAll(int FDNum, char *Buf, size_t Size) {
    size_t Got = 0;
    while (Got < Size) {
      ssize_t N = ::recv(FDNum, Buf + Got, Size - Got, 0);
      if (N == 0)
        return Got;
      if (N < 0) {
        if (errno == EINTR)
          continue;
        return make_error<StringError>(std::string("recv: ") + strerror(errno));
      }
      Got += N;
    }
    return Got;
  }

  /// A framed message, ready to write to the socket.
  static std::vector<char> frame(Opcode Op, uint64_t SeqNo, ExecutorAddr Tag,
                                 std::string_view Payload) {
    std::vector<char> Buf(HeaderSize + Payload.size());
    MsgHeader::encode(Buf.data(), Op, SeqNo, Tag.toPtr<void *>(),
                      Payload.size());
    if (!Payload.empty())
      memcpy(Buf.data() + HeaderSize, Payload.data(), Payload.size());
    return Buf;
  }

  static Error writeFrame(int FDNum, Opcode Op, uint64_t SeqNo,
                          ExecutorAddr Tag = ExecutorAddr(),
                          std::string_view Payload = {}) {
    auto Buf = frame(Op, SeqNo, Tag, Payload);
    return sendAll(FDNum, Buf.data(), Buf.size());
  }

  static Expected<Frame> readFrame(int FDNum) {
    char H[HeaderSize];
    auto N = recvAll(FDNum, H, HeaderSize);
    if (!N)
      return N.takeError();
    if (*N != HeaderSize)
      return make_error<StringError>(
          "peer closed before a full header arrived");

    Frame F;
    F.Fields = MsgHeader::decode(H);
    if (F.Fields.MsgSize < HeaderSize)
      return make_error<StringError>("framed message size is below the header");

    if (size_t PayloadSize = F.Fields.MsgSize - HeaderSize) {
      F.Payload.resize(PayloadSize);
      auto M = recvAll(FDNum, F.Payload.data(), PayloadSize);
      if (!M)
        return M.takeError();
      if (*M != PayloadSize)
        return make_error<StringError>("peer closed mid-payload");
    }
    return F;
  }

  /// The payload a controller sends to hang up, via the CA's own encoder.
  static std::vector<char> hangupPayload(Error Err) {
    auto P = CA::encodeHangup(std::move(Err));
    return std::vector<char>(P.data(), P.data() + P.size());
  }
};

} // namespace orc_rt

namespace {

std::string_view view(const std::vector<char> &V) {
  return {V.data(), V.size()};
}

// Wrapper that echoes its arguments back as the result.
void echoWrapper(orc_rt_SessionRef S, orc_rt_WrapperFunctionBuffer ArgBytes,
                 orc_rt_WrapperFunctionReturn Return, uint64_t CallId) {
  Return(S, ArgBytes, CallId);
}

// A wrapper that defers its result: it stashes everything needed to return, so
// a test can complete the call later. A plain function pointer has nowhere to
// put context, hence the global.
struct DeferredCall {
  orc_rt_SessionRef S = nullptr;
  orc_rt_WrapperFunctionBuffer ArgBytes{};
  orc_rt_WrapperFunctionReturn Return = nullptr;
  uint64_t CallId = 0;
};
DeferredCall Deferred;

void deferringWrapper(orc_rt_SessionRef S,
                      orc_rt_WrapperFunctionBuffer ArgBytes,
                      orc_rt_WrapperFunctionReturn Return, uint64_t CallId) {
  // ArgBytes is stashed rather than disposed: it goes back out as the result.
  Deferred = DeferredCall{S, ArgBytes, Return, CallId};
}

} // namespace

TEST_F(SocketSimpleRemoteCATest, SetupIsSentOnConnect) {
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));

  auto F = readFrame(Far.get());
  ASSERT_TRUE(!!F) << toString(F.takeError());
  EXPECT_EQ(F->opcode(), Opcode::Setup);
  EXPECT_EQ(F->seqNo(), 0u) << "setup carries no sequence number";
  EXPECT_FALSE(!!F->tag()) << "setup carries no handler tag";
  EXPECT_FALSE(F->Payload.empty()) << "setup carries the bootstrap payload";
}

TEST_F(SocketSimpleRemoteCATest, ControllerCallIsFramedAndResultCompletesIt) {
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));
  ASSERT_TRUE(!!readFrame(Far.get())) << "expected setup first";

  std::future<std::string> Result;
  S.callController(
      [SetResult = waitFor(Result)](WrapperFunctionBuffer R) mutable {
        SetResult(std::string(R.data(), R.size()));
      },
      nullptr, WrapperFunctionBuffer::copyFrom("args", 4));

  auto Call = readFrame(Far.get());
  ASSERT_TRUE(!!Call) << toString(Call.takeError());
  EXPECT_EQ(Call->opcode(), Opcode::Call);
  EXPECT_NE(Call->seqNo(), 0u)
      << "a call awaiting a result needs a sequence no.";
  EXPECT_EQ(Call->payload(), "args");

  // Reply under the same sequence number; the handler completes.
  ASSERT_FALSE(!!writeFrame(Far.get(), Opcode::Result, Call->seqNo(),
                            ExecutorAddr(), "reply"));
  EXPECT_EQ(Result.get(), "reply");
}

TEST_F(SocketSimpleRemoteCATest, ControllerInitiatedCallReturnsAResult) {
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));
  ASSERT_TRUE(!!readFrame(Far.get())) << "expected setup first";

  // A Call names the wrapper by tag, and its sequence number is the call id.
  auto Tag = ExecutorAddr::fromPtr(reinterpret_cast<void *>(echoWrapper));
  ASSERT_FALSE(
      !!writeFrame(Far.get(), Opcode::Call, /*SeqNo=*/42, Tag, "world"));

  auto R = readFrame(Far.get());
  ASSERT_TRUE(!!R) << toString(R.takeError());
  EXPECT_EQ(R->opcode(), Opcode::Result);
  EXPECT_EQ(R->seqNo(), 42u) << "the result must carry the call id back";
  EXPECT_EQ(R->payload(), "world");
}

TEST_F(SocketSimpleRemoteCATest, EmptyPayloadsRoundTrip) {
  // A message whose size is exactly the header. Nothing is pending the moment
  // the header is decoded, which is the case that stops "header full" and
  // "header decoded" being the same state, in both directions: the echoed
  // result is empty too.
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));
  ASSERT_TRUE(!!readFrame(Far.get())) << "expected setup first";

  auto Tag = ExecutorAddr::fromPtr(reinterpret_cast<void *>(echoWrapper));
  ASSERT_FALSE(!!writeFrame(Far.get(), Opcode::Call, /*SeqNo=*/8, Tag));

  auto R = readFrame(Far.get());
  ASSERT_TRUE(!!R) << toString(R.takeError());
  EXPECT_EQ(R->opcode(), Opcode::Result);
  EXPECT_EQ(R->seqNo(), 8u);
  EXPECT_TRUE(R->Payload.empty());

  // Still framing correctly afterwards: an empty message must not desynchronise
  // the stream.
  ASSERT_FALSE(
      !!writeFrame(Far.get(), Opcode::Call, /*SeqNo=*/9, Tag, "after"));
  auto R2 = readFrame(Far.get());
  ASSERT_TRUE(!!R2) << toString(R2.takeError());
  EXPECT_EQ(R2->seqNo(), 9u);
  EXPECT_EQ(R2->payload(), "after");
}

TEST_F(SocketSimpleRemoteCATest, PartialWritesAreReassembled) {
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));
  ASSERT_TRUE(!!readFrame(Far.get())) << "expected setup first";

  // One byte at a time, so the reactor sees the message split in every possible
  // place -- including mid-header.
  auto Tag = ExecutorAddr::fromPtr(reinterpret_cast<void *>(echoWrapper));
  auto Buf = frame(Opcode::Call, /*SeqNo=*/7, Tag, "drip");
  for (char C : Buf)
    ASSERT_FALSE(!!sendAll(Far.get(), &C, 1));

  auto R = readFrame(Far.get());
  ASSERT_TRUE(!!R) << toString(R.takeError());
  EXPECT_EQ(R->opcode(), Opcode::Result);
  EXPECT_EQ(R->seqNo(), 7u);
  EXPECT_EQ(R->payload(), "drip");
}

TEST_F(SocketSimpleRemoteCATest, NothingIsQueuedBehindTheHangup) {
  // The hang-up must be the last message on the wire, and sendWrapperResult
  // does no state check -- the base leaves that to the transport. So a result
  // that arrives once teardown has begun must be dropped rather than appended.
  //
  // The window is between beginTeardown queueing the hang-up and the reactor
  // finishing, which is narrow. It is held open here by never reading the far
  // end until the very end: the reactor parks in would-block with a part-sent
  // message, so nothing drains while the late result is submitted.
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));
  ASSERT_TRUE(!!readFrame(Far.get())) << "expected setup first";

  // Echo back more than the socket buffer can hold, so the reactor stalls
  // part-way through sending the result.
  const std::string Big(1 << 20, 'x');
  auto EchoTag = ExecutorAddr::fromPtr(reinterpret_cast<void *>(echoWrapper));
  ASSERT_FALSE(
      !!writeFrame(Far.get(), Opcode::Call, /*SeqNo=*/1, EchoTag, Big));

  // A second call that will not have returned when teardown starts.
  Deferred = DeferredCall{};
  auto DeferTag =
      ExecutorAddr::fromPtr(reinterpret_cast<void *>(deferringWrapper));
  ASSERT_FALSE(
      !!writeFrame(Far.get(), Opcode::Call, /*SeqNo=*/2, DeferTag, "late"));

  // Wait for the wrapper to run without draining the socket.
  while (Deferred.Return == nullptr)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // Queues the hang-up and latches the queue. Returns without waiting for the
  // reactor, which is still stalled.
  S.detach([] {});

  // Too late: this must not reach the wire.
  Deferred.Return(Deferred.S, Deferred.ArgBytes, Deferred.CallId);

  // Now drain. The stalled result completes, then the hang-up, then EOF.
  auto R = readFrame(Far.get());
  ASSERT_TRUE(!!R) << toString(R.takeError());
  EXPECT_EQ(R->opcode(), Opcode::Result);
  EXPECT_EQ(R->Payload.size(), Big.size());

  auto H = readFrame(Far.get());
  ASSERT_TRUE(!!H) << toString(H.takeError());
  EXPECT_EQ(H->opcode(), Opcode::Hangup) << "the hang-up did not come last";

  char Byte = 0;
  auto N = recvAll(Far.get(), &Byte, 1);
  ASSERT_TRUE(!!N) << toString(N.takeError());
  EXPECT_EQ(*N, 0u) << "a message was queued behind the hang-up";
}

TEST_F(SocketSimpleRemoteCATest, HangupFromControllerEndsTheSession) {
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  std::future<Error> Disconnected;
  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  S.setOnDisconnect(waitFor(Disconnected));
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));
  ASSERT_TRUE(!!readFrame(Far.get())) << "expected setup first";

  // An orderly hang-up carries a success Error as its reason.
  ASSERT_FALSE(!!writeFrame(Far.get(), Opcode::Hangup, 0, ExecutorAddr(),
                            view(hangupPayload(Error::success()))));

  EXPECT_FALSE(!!Disconnected.get()) << "an orderly hang-up is not an error";
}

TEST_F(SocketSimpleRemoteCATest, PeerReasonSurvivesAStalledSendQueue) {
  // A hang-up reason from the controller must be what ends the session, even
  // when we have messages queued that can no longer be delivered. Sending first
  // would fail with EPIPE and report that instead, losing the reason.
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  std::future<Error> Disconnected;
  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  S.setOnDisconnect(waitFor(Disconnected));
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));
  ASSERT_TRUE(!!readFrame(Far.get())) << "expected setup first";

  // Echo back more than the socket will hold, and never read it, so the reactor
  // is left with a part-sent message queued.
  const std::string Big(1 << 20, 'x');
  auto Tag = ExecutorAddr::fromPtr(reinterpret_cast<void *>(echoWrapper));
  ASSERT_FALSE(!!writeFrame(Far.get(), Opcode::Call, /*SeqNo=*/1, Tag, Big));

  // Hang up with a reason, then vanish. The reason is buffered on our side of
  // the socket and survives the close.
  ASSERT_FALSE(!!writeFrame(
      Far.get(), Opcode::Hangup, 0, ExecutorAddr(),
      view(hangupPayload(make_error<StringError>("controller ran out of x")))));
  ::close(Far.release());

  auto Err = Disconnected.get();
  ASSERT_TRUE(!!Err) << "a hang-up carrying a reason ends with that reason";
  EXPECT_EQ(toString(std::move(Err)), "controller ran out of x");
}

TEST_F(SocketSimpleRemoteCATest, TruncatedMessageIsReportedAsAnError) {
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  std::future<Error> Disconnected;
  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  S.setOnDisconnect(waitFor(Disconnected));
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));
  ASSERT_TRUE(!!readFrame(Far.get())) << "expected setup first";

  // Half a header, then gone: distinguishable from a close at a boundary.
  char Half[HeaderSize / 2] = {};
  ASSERT_FALSE(!!sendAll(Far.get(), Half, sizeof(Half)));
  ::close(Far.release());

  auto Err = Disconnected.get();
  EXPECT_TRUE(!!Err) << "a truncated message must not look like a clean end";
  EXPECT_EQ(toString(std::move(Err)),
            "Connection closed without a hang-up message");
}

TEST_F(SocketSimpleRemoteCATest, PeerCloseWithoutAHangupIsAnError) {
  // A peer that means to end the session says so with a hang-up. Vanishing
  // instead means it crashed or was killed, which must not be reported as a
  // clean shutdown -- a controller that dies has to fail the session.
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  std::future<Error> Disconnected;
  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  S.setOnDisconnect(waitFor(Disconnected));
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));
  ASSERT_TRUE(!!readFrame(Far.get())) << "expected setup first";

  // Closed between messages, so nothing is truncated -- it is simply gone.
  ::close(Far.release());

  auto Err = Disconnected.get();
  ASSERT_TRUE(!!Err) << "a silent close is not an orderly end";
  EXPECT_EQ(toString(std::move(Err)),
            "Connection closed without a hang-up message");
}

TEST_F(SocketSimpleRemoteCATest, MessageSizeBelowHeaderIsRejected) {
  auto P = makePair();
  ASSERT_TRUE(!!P) << toString(P.takeError());
  SocketHandle Near(P->first), Far(P->second);

  std::future<Error> Disconnected;
  Session S(mockExecutorProcessInfo(), inlineDispatch, noErrors);
  S.setOnDisconnect(waitFor(Disconnected));
  ASSERT_FALSE(
      !!S.tryAttach<SocketSimpleRemoteCA>(BootstrapInfo(S), std::move(Near)));
  ASSERT_TRUE(!!readFrame(Far.get())) << "expected setup first";

  // A size that excludes its own header would make the payload length negative.
  char H[HeaderSize] = {};
  endian_write<uint64_t>(H, HeaderSize - 1, endian::little);
  ASSERT_FALSE(!!sendAll(Far.get(), H, sizeof(H)));

  auto Err = Disconnected.get();
  EXPECT_TRUE(!!Err);
  EXPECT_EQ(toString(std::move(Err)), "Message size smaller than its header");
}
