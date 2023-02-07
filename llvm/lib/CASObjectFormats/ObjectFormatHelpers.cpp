//===- CASObjectFormats/ObjectFormatHelpers.cpp ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CASObjectFormats/ObjectFormatHelpers.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/CASObjectFormats/Encoding.h"
#include "llvm/Support/EndianStream.h"

// FIXME: For jitlink::x86_64::writeOperand(). Should use a generic version.
#include "llvm/ExecutionEngine/JITLink/x86_64.h"

using namespace llvm;
using namespace llvm::casobjectformats;
using namespace llvm::casobjectformats::helpers;

static Error createArchError(Triple::ArchType Arch, StringRef Prefix = "") {
  Triple TT;
  TT.setArch(Arch);
  return createStringError(inconvertibleErrorCode(),
                           Prefix + "unhandled arch: '" + TT.getArchName() +
                               "'");
}

Error helpers::checkArch(Triple::ArchType Arch) {
  // FIXME: Handle arm64!
  if (Arch == Triple::x86_64)
    return Error::success();

  return createArchError(Arch);
}

// FIXME: Copy/pasted from X86AsmBackend::writeNopData.
static void writeX86NopData(raw_ostream &OS, uint64_t Count) {
  static const char Nops[10][11] = {
      // nop
      "\x90",
      // xchg %ax,%ax
      "\x66\x90",
      // nopl (%[re]ax)
      "\x0f\x1f\x00",
      // nopl 0(%[re]ax)
      "\x0f\x1f\x40\x00",
      // nopl 0(%[re]ax,%[re]ax,1)
      "\x0f\x1f\x44\x00\x00",
      // nopw 0(%[re]ax,%[re]ax,1)
      "\x66\x0f\x1f\x44\x00\x00",
      // nopl 0L(%[re]ax)
      "\x0f\x1f\x80\x00\x00\x00\x00",
      // nopl 0L(%[re]ax,%[re]ax,1)
      "\x0f\x1f\x84\x00\x00\x00\x00\x00",
      // nopw 0L(%[re]ax,%[re]ax,1)
      "\x66\x0f\x1f\x84\x00\x00\x00\x00\x00",
      // nopw %cs:0L(%[re]ax,%[re]ax,1)
      "\x66\x2e\x0f\x1f\x84\x00\x00\x00\x00\x00",
  };

  const uint64_t MaxNopLength = 16;

  // Emit as many MaxNopLength NOPs as needed, then emit a NOP of the remaining
  // length.
  do {
    const uint8_t ThisNopLength = (uint8_t)std::min(Count, MaxNopLength);
    const uint8_t Prefixes = ThisNopLength <= 10 ? 0 : ThisNopLength - 10;
    for (uint8_t i = 0; i < Prefixes; i++)
      OS << '\x66';
    const uint8_t Rest = ThisNopLength - Prefixes;
    if (Rest != 0)
      OS.write(Nops[Rest - 1], Rest);
    Count -= ThisNopLength;
  } while (Count != 0);
}

Error helpers::writeOperandForArch(Triple::ArchType Arch, jitlink::Edge::Kind K,
                                   uint64_t OperandValue, char *FixupPtr) {
  if (Arch == Triple::x86_64)
    return jitlink::x86_64::writeOperand(K, OperandValue, FixupPtr);

  return createArchError(Arch, "writeOperandForArch");
}

Error helpers::writeNopForArch(Triple::ArchType Arch, raw_ostream &OS,
                               uint64_t Count) {
  if (Arch == Triple::x86_64) {
    writeX86NopData(OS, Count);
    return Error::success();
  }

  return createArchError(Arch, "writeNopForArch");
}

Expected<StringRef> helpers::canonicalizeContent(
    Triple::ArchType Arch, StringRef Content, SmallVectorImpl<char> &Storage,
    ArrayRef<data::Fixup> FixupsToZero, Optional<Align> TrailingNopsAlignment) {
  SmallVectorImpl<char> *MutableContent = nullptr;
  auto initializeMutableContent = [&]() {
    if (MutableContent)
      return;
    MutableContent = &Storage;
    assert(MutableContent->empty());
    MutableContent->append(Content.begin(), Content.end());
  };
  auto getFixupPtr = [&](const data::Fixup &F) -> char * {
    initializeMutableContent();
    return &(*MutableContent)[F.Offset];
  };

  for (const data::Fixup &F : FixupsToZero)
    if (F.Kind >= jitlink::Edge::FirstRelocation)
      if (Error E = writeOperandForArch(Arch, F.Kind, 0, getFixupPtr(F)))
        return std::move(E);

  if (TrailingNopsAlignment &&
      !isAligned(*TrailingNopsAlignment, Content.size())) {
    initializeMutableContent();

    raw_svector_ostream OS(*MutableContent);
    if (Error E = writeNopForArch(
            Arch, OS,
            offsetToAlignment(MutableContent->size(), *TrailingNopsAlignment)))
      return std::move(E);
  }

  return MutableContent ? StringRef(Storage.begin(), Storage.size()) : Content;
}

bool helpers::compareSymbolsBySemanticsAnd(
    const jitlink::Symbol *LHS, const jitlink::Symbol *RHS,
    function_ref<bool(const jitlink::Symbol *, const jitlink::Symbol *)>
        NextCompare) {
  if (LHS == RHS)
    return NextCompare(LHS, RHS);

  // Sort by name, putting anonymous symbols last.
  if (LHS->hasName() != RHS->hasName())
    return LHS->hasName() > RHS->hasName();
  if (LHS->hasName())
    if (int Diff = LHS->getName().compare(RHS->getName()))
      return Diff < 0;

  // Put external symbols last, stopping if both are external.
  if (LHS->isExternal() != RHS->isExternal())
    return LHS->isExternal() < RHS->isExternal();
  if (LHS->isExternal())
    return NextCompare(LHS, RHS);

  // Put absolute symbols after defined ones. Sort by symbol size if they're
  // both absolute.
  if (LHS->isAbsolute() != RHS->isAbsolute())
    return LHS->isAbsolute() < RHS->isAbsolute();
  if (LHS->isAbsolute()) {
    if (LHS->getSize() != RHS->getSize())
      return LHS->getSize() < RHS->getSize();
    return NextCompare(LHS, RHS);
  }

  // Only defined symbols should remain.
  assert(LHS->isDefined() && "Expected defined symbol");
  assert(RHS->isDefined() && "Expected defined symbol");

  // Compare section name.
  const jitlink::Block &LB = LHS->getBlock();
  const jitlink::Block &RB = RHS->getBlock();
  if (&LB.getSection() != &RB.getSection())
    if (int Diff = LB.getSection().getName().compare(RB.getSection().getName()))
      return Diff < 0;

  // Compare symbol size.
  if (LHS->getSize() != RHS->getSize())
    return LHS->getSize() < RHS->getSize();

  // If it's the same block, compare by symbol offset.
  if (&LB == &RB) {
    if (LHS->getOffset() != RHS->getOffset())
      return LHS->getOffset() < RHS->getOffset();
    return NextCompare(LHS, RHS);
  }

  // Sort structurally by the block.
  if (LB.edges_size() != RB.edges_size())
    return LB.edges_size() < RB.edges_size();
  if (LB.getSize() != RB.getSize())
    return LB.getSize() < RB.getSize();

  // Compare block content.
  if (LB.isZeroFill() != RB.isZeroFill())
    return LB.isZeroFill() < RB.isZeroFill();
  if (LB.isZeroFill())
    return NextCompare(LHS, RHS);

  // FIXME: This could expensive. Maybe this should only be done sometimes
  // (when symbols are mergeable by content?).
  //
  // FIXME: Fixups have not been zeroed out yet so this isn't going to match
  // across TUs.
  if (int Diff = StringRef(LB.getContent().begin(), LB.getSize())
                     .compare(StringRef(RB.getContent().begin(), RB.getSize())))
    return Diff < 0;
  return NextCompare(LHS, RHS);
}

bool helpers::compareSymbolsByLinkageAndSemantics(const jitlink::Symbol *LHS,
                                                  const jitlink::Symbol *RHS) {
  if (LHS == RHS)
    return false;

  // Put locals last.
  if (LHS->getScope() != RHS->getScope())
    return LHS->getScope() < RHS->getScope();

  // Put strong symbols before weak symbols.
  if (LHS->getLinkage() != RHS->getLinkage())
    return LHS->getLinkage() < RHS->getLinkage();

  // Put no-dead-strip symbols ahead of others.
  if (LHS->isLive() != RHS->isLive())
    return LHS->isLive() > RHS->isLive();

  return compareSymbolsBySemanticsAnd(
      LHS, RHS,
      [](const jitlink::Symbol *, const jitlink::Symbol *) { return false; });
}

bool helpers::compareSymbolsByAddress(const jitlink::Symbol *LHS,
                                      const jitlink::Symbol *RHS) {
  if (LHS == RHS)
    return false;

  if (LHS->isExternal() != RHS->isExternal())
    return LHS->isExternal() < RHS->isExternal();

  JITTargetAddress LAddr = LHS->getAddress();
  JITTargetAddress RAddr = RHS->getAddress();
  if (LAddr != RAddr)
    return LAddr < RAddr;

  return LHS->getSize() < RHS->getSize();
}

