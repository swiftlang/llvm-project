//===- llvm/CodeGen/DwarfFile.cpp - Dwarf Debug Framework -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "DwarfFile.h"
#include "DwarfCompileUnit.h"
#include "DwarfDebug.h"
#include "DwarfUnit.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/MC/MCStreamer.h"
#include <cstdint>

using namespace llvm;

DwarfFile::DwarfFile(AsmPrinter *AP, StringRef Pref, BumpPtrAllocator &DA)
    : Asm(AP), StrPool(DA, *Asm, Pref) {
  Abbrevs.emplace_back(std::make_unique<DIEAbbrevSet>(AbbrevAllocator));
}

void DwarfFile::addUnit(std::unique_ptr<DwarfCompileUnit> U) {
  CUs.push_back(std::move(U));
}

void DwarfFile::addUnit(std::unique_ptr<DwarfTypeUnit> U) {
  TUs.push_back(std::move(U));
}

// Emit the various dwarf units to the unit section USection with
// the abbreviations going into ASection.
void DwarfFile::emitUnits(bool UseOffsets) {
  bool FirstContribution = true;
  auto Emit = [&FirstContribution, UseOffsets, this](auto &UnitCollection) {
    for (const auto &TheU : UnitCollection) {
      getOrCreateAbbrevSectionStart();
      TheU.get()->setAbbrevSectionBegin(AbbrevSectionStart);
      // If there is only one Abbreviation contribution or we are working with
      // the first compile unit, we can avoid label arithmetic by setting the
      // offset to the start of the section.
      if (FirstContribution || Abbrevs.size() == 1) {
        FirstContribution = false;
        TheU.get()->setAbbrevOffset(AbbrevSectionStart);
      }
      emitUnit(TheU.get(), UseOffsets);
    }
  };
  Emit(TUs);
  Emit(CUs);
}

void DwarfFile::emitUnit(DwarfUnit *TheU, bool UseOffsets) {
  if (TheU->getCUNode()->isDebugDirectivesOnly())
    return;

  MCSection *S = TheU->getSection();

  if (!S)
    return;

  // Skip CUs that ended up not being needed (split CUs that were abandoned
  // because they added no information beyond the non-split CU)
  if (TheU->getUnitDie().values().empty())
    return;

  Asm->OutStreamer->switchSection(S);
  TheU->emitHeader(UseOffsets);
  Asm->emitDwarfDIE(TheU->getUnitDie());

  if (MCSymbol *EndLabel = TheU->getEndLabel())
    Asm->OutStreamer->emitLabel(EndLabel);
}

MCSymbol *DwarfFile::getOrCreateAbbrevSectionStart() {
  if (!AbbrevSectionStart)
    AbbrevSectionStart = Asm->createTempSymbol("debug_abbrev_begin");
  return AbbrevSectionStart;
}

// Compute the size and offset for each DIE.
void DwarfFile::computeSizeAndOffsets() {
  // Offset from the first CU in the debug info section is 0 initially.
  uint64_t SecOffset = 0;

  auto Compute = [&SecOffset, this](auto &UnitCollection) {
    // Iterate over each compile unit and set the size and offsets for each
    // DIE within each compile unit. All offsets are CU relative.
    for (const auto &TheU : UnitCollection) {
      if (TheU->getCUNode()->isDebugDirectivesOnly())
        continue;

      // Skip CUs that ended up not being needed (split CUs that were abandoned
      // because they added no information beyond the non-split CU)
      if (TheU->getUnitDie().values().empty())
        return;

      TheU->setDebugSectionOffset(SecOffset);
      SecOffset += computeSizeAndOffsetsForUnit(TheU.get());
      // Only push_back a new .debug_abbrev contribution when emitting
      // CAS-friendly debug info, and make sure to not add an extra compile
      // unit. An initial Abbreviation contribution is pushed back when the
      // DwarfFile is created because there are cases where the compile units
      // are emitted without computing size and offsets for each DIE.
      auto MaxAbbrevs = CUs.size() + TUs.size();
      if (Abbrevs.size() != MaxAbbrevs &&
          TheU->getCUNode()->getCasFriendlinessKind() ==
              DICompileUnit::DebugAbbrev) {
        Abbrevs.emplace_back(std::make_unique<DIEAbbrevSet>(AbbrevAllocator));
      }
    }
  };
  Compute(TUs);
  Compute(CUs);
  if (SecOffset > UINT32_MAX && !Asm->isDwarf64())
    report_fatal_error("The generated debug information is too large "
                       "for the 32-bit DWARF format.");
}

unsigned DwarfFile::computeSizeAndOffsetsForUnit(DwarfUnit *TheU) {
  // CU-relative offset is reset to 0 here.
  unsigned Offset = Asm->getUnitLengthFieldByteSize() + // Length of Unit Info
                    TheU->getHeaderSize();              // Unit-specific headers

  // The return value here is CU-relative, after laying out
  // all of the CU DIE.
  return computeSizeAndOffset(TheU->getUnitDie(), Offset);
}

// Compute the size and offset of a DIE. The offset is relative to start of the
// CU. It returns the offset after laying out the DIE.
unsigned DwarfFile::computeSizeAndOffset(DIE &Die, unsigned Offset) {
  return Die.computeOffsetsAndAbbrevs(Asm->getDwarfFormParams(), Abbrevs,
                                      Offset);
}

void DwarfFile::emitAbbrevs(MCSection *Section) {
  // If the abbreviation vector only has one .debug_abbrev contribution and the
  // contribution has non-zero abbreviations in use, emit it without emitting
  // any labels. This is done because there are some targets do not support
  // labels inside debug sections.
  if (Abbrevs.size() == 1 && !Abbrevs[0]->isAbbreviationListEmpty()) {
    Asm->OutStreamer->switchSection(Section);
    Abbrevs[0]->Emit(Asm);
    return;
  }
  for (unsigned I = 0; I < Abbrevs.size(); I++) {
    // If the abbreviation contribution is empty, do not
    // switch sections or emit labels.
    if (Abbrevs[I]->isAbbreviationListEmpty())
      continue;

    auto *TheU = [&]() -> DwarfUnit * {
      if (I < TUs.size())
        return TUs[I].get();
      auto CUIndex = I - TUs.size();
      return CUs[CUIndex].get();
    }();

    Asm->OutStreamer->switchSection(Section);
    MCSymbol *AbbrevStartSym = getOrCreateAbbrevSectionStart();
    // Do not emit the same label again, AbbrevStartSym should be the same for
    // all abbreviation contributions, it denotes the start of the
    // abbreviation Section.
    if (!AbbrevStartEmitted) {
      AbbrevStartEmitted = true;
      Asm->OutStreamer->emitLabel(AbbrevStartSym);
    }
    TheU->setAbbrevSectionBegin(AbbrevSectionStart);
    // If this is the first abbreviation contribution, we do not emit label
    // differences, therefore the abbreviation offset is set to be the same
    // as the start of the abbreviation section.
    if (I == 0)
      TheU->setAbbrevOffset(AbbrevSectionStart);
    else
      Asm->OutStreamer->emitLabel(TheU->getOrCreateAbbrevOffset());
    Abbrevs[I]->Emit(Asm);
  }
}

// Emit strings into a string section.
void DwarfFile::emitStrings(MCSection *StrSection, MCSection *OffsetSection,
                            bool UseRelativeOffsets) {
  StrPool.emit(*Asm, StrSection, OffsetSection, UseRelativeOffsets);
}

bool DwarfFile::addScopeVariable(LexicalScope *LS, DbgVariable *Var) {
  auto &ScopeVars = ScopeVariables[LS];
  const DILocalVariable *DV = Var->getVariable();
  if (unsigned ArgNum = DV->getArg()) {
    auto Cached = ScopeVars.Args.find(ArgNum);
    if (Cached == ScopeVars.Args.end())
      ScopeVars.Args[ArgNum] = Var;
    else {
      Cached->second->addMMIEntry(*Var);
      return false;
    }
  } else {
    ScopeVars.Locals.push_back(Var);
  }
  return true;
}

void DwarfFile::addScopeLabel(LexicalScope *LS, DbgLabel *Label) {
  SmallVectorImpl<DbgLabel *> &Labels = ScopeLabels[LS];
  Labels.push_back(Label);
}

std::pair<uint32_t, RangeSpanList *>
DwarfFile::addRange(const DwarfCompileUnit &CU, SmallVector<RangeSpan, 2> R) {
  CURangeLists.push_back(
      RangeSpanList{Asm->createTempSymbol("debug_ranges"), &CU, std::move(R)});
  return std::make_pair(CURangeLists.size() - 1, &CURangeLists.back());
}
