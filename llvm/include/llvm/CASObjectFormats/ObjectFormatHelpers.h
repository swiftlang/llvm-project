//===- llvm/CASObjectFormats/ObjectFormatHelpers.h --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CASOBJECTFORMATS_OBJECTFORMATHELPERS_H
#define LLVM_CASOBJECTFORMATS_OBJECTFORMATHELPERS_H

#include "llvm/ADT/Triple.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CASObjectFormats/Data.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"

namespace llvm {
namespace casobjectformats {
namespace helpers {

/// Check that the helpers support this architecture.
Error checkArch(Triple::ArchType Arch);

/// Check that the helpers support \p TT's architecture.
inline Error checkArch(const Triple &TT) { return checkArch(TT.getArch()); }

/// Write out an operand value. Errors if \p Arch isn't handled.
Error writeOperandForArch(Triple::ArchType Arch, jitlink::Edge::Kind K,
                          uint64_t OperandValue, char *FixupPtr);

/// Append some nops. Errors if \p Arch isn't handled.
Error writeNopForArch(Triple::ArchType Arch, raw_ostream &OS, uint64_t Count);

/// Return a canonicalized version of \p Content. Either returns \p Content, or
/// fills in \p Storage, canonicalizes it, and returns a reference into there.
///
/// - Pass \p Fixups to zero out fixups.
/// - Pass \p TrailingNopsAlignment to add nops up to an alignment.
Expected<StringRef>
canonicalizeContent(Triple::ArchType Arch, StringRef Content,
                    SmallVectorImpl<char> &Storage,
                    ArrayRef<data::Fixup> FixupsToZero = None,
                    Optional<Align> TrailingNopsAlignment = None);

/// Helpers to sort jitlink::Symbols into stable ordering so the same object
/// will always have the same symbol ordering between builds.
bool compareSymbolsBySemanticsAnd(
    const jitlink::Symbol *LHS, const jitlink::Symbol *RHS,
    function_ref<bool(const jitlink::Symbol *, const jitlink::Symbol *)>
        NextCompare);

bool compareSymbolsByLinkageAndSemantics(const jitlink::Symbol *LHS,
                                         const jitlink::Symbol *RHS);

bool compareSymbolsByAddress(const jitlink::Symbol *LHS,
                             const jitlink::Symbol *RHS);

} // end namespace helpers
} // end namespace casobjectformats
} // end namespace llvm

#endif // LLVM_CASOBJECTFORMATS_OBJECTFORMATHELPERS_H
