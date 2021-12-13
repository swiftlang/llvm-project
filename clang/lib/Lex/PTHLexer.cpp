//===- PTHLexer.cpp - Lex from a token stream -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the PTHLexer interface.
//
//===----------------------------------------------------------------------===//

#include "clang/Lex/PTHLexer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/FileSystemStatCache.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Basic/Version.h"
#include "clang/Lex/LexDiagnostic.h"
#include "clang/Lex/PTHManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Token.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/HierarchicalTreeBuilder.h"
#include "llvm/Support/DJB.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/OnDiskHashTable.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <utility>

using namespace clang;
using namespace llvm::support;

static const unsigned StoredTokenSize = 2 + 2 + 4 + 4 + 4;

//===----------------------------------------------------------------------===//
// PTHLexer methods.
//===----------------------------------------------------------------------===//

PTHLexer::PTHLexer(PTHHandler &Handler, Preprocessor &PP, FileID FID)
    : PreprocessorLexer(&PP, FID), TokBuf(Handler.TokenStream),
      CurPtr(Handler.TokenStream), PPCond(Handler.PPCondTable),
      CurPPCondPtr(Handler.PPCondTable), Handler(Handler) {
  FileStartLoc = PP.getSourceManager().getLocForStartOfFile(FID);
}

IdentifierInfo *PTHLexer::getOrCreateIdentifier(uint32_t IdentifierID,
                                                Token &T) {
  assert(IdentifierID < Handler.IdentifierInfoCache.size());
  IdentifierInfo *&II = Handler.IdentifierInfoCache[IdentifierID];
  if (II) {
    T.setIdentifierInfo(II);

    // FIXME: copy/paste from Preprocessor::LookUpIdentifierInfo().
    if (PP->getLangOpts().MSVCCompat && II->isCPlusPlusOperatorKeyword() &&
        PP->getSourceManager().isInSystemHeader(T.getLocation()))
      T.setKind(tok::identifier);
    else
      T.setKind(II->getTokenID());

    return II;
  }

  uint32_t StringOffset = endian::read<uint32_t, little, aligned>(
      Handler.IdentifierTable + sizeof(uint32_t) * (IdentifierID + 1));
  T.setRawIdentifierData((const char *)(Handler.StringsTable + StringOffset));
  II = PP->LookUpIdentifierInfo(T);
  return II;
}

bool PTHLexer::Lex(Token &Tok) {
  //===--------------------------------------==//
  // Read the raw token data.
  //===--------------------------------------==//
  using namespace llvm::support;

  // Shadow CurPtr into an automatic variable.
  const unsigned char *CurPtrShadow = CurPtr;

  // Read in the data for the token.
  unsigned Word0 = endian::readNext<uint32_t, little, aligned>(CurPtrShadow);
  unsigned Len = endian::readNext<uint32_t, little, aligned>(CurPtrShadow);
  uint32_t IdentifierID =
      endian::readNext<uint32_t, little, aligned>(CurPtrShadow);
  uint32_t FileOffset =
      endian::readNext<uint32_t, little, aligned>(CurPtrShadow);

  tok::TokenKind TKind = (tok::TokenKind)(Word0 & 0xFFFF);
  Token::TokenFlags TFlags = (Token::TokenFlags)((Word0 >> 16) & 0xFFFF);

  CurPtr = CurPtrShadow;

  //===--------------------------------------==//
  // Construct the token itself.
  //===--------------------------------------==//

  Tok.startToken();
  Tok.setKind(TKind);
  Tok.setFlag(TFlags);
  assert(!LexingRawMode);
  Tok.setLocation(FileStartLoc.getLocWithOffset(FileOffset));
  Tok.setLength(Len);

  // Handle identifiers.
  if (Tok.isLiteral()) {
    Tok.setLiteralData((const char *)(Handler.StringsTable + IdentifierID));
  } else if (Tok.isAnyIdentifier()) {
    MIOpt.ReadToken();
    IdentifierInfo *II = getOrCreateIdentifier(IdentifierID, Tok);

    // FIXME: This is missing the code completion case from
    // Lexer::LexIdentifier(). Should be looking ahead here...
    if (II->isHandleIdentifierCase())
      return PP->HandleIdentifier(Tok);

    return true;
  }

  //===--------------------------------------==//
  // Process the token.
  //===--------------------------------------==//
  if (TKind == tok::eof) {
    // Save the end-of-file token.
    EofToken = Tok;

    assert(!ParsingPreprocessorDirective);
    assert(!LexingRawMode);

    return LexEndOfFile(Tok);
  }

  if (TKind == tok::hash && Tok.isAtStartOfLine()) {
    LastHashTokPtr = CurPtr - StoredTokenSize;
    assert(!LexingRawMode);
    PP->HandleDirective(Tok);

    return false;
  }

  if (TKind == tok::eod) {
    assert(ParsingPreprocessorDirective);
    ParsingPreprocessorDirective = false;
    return true;
  }

  MIOpt.ReadToken();
  return true;
}

bool PTHLexer::LexEndOfFile(Token &Result) {
  // If we hit the end of the file while parsing a preprocessor directive,
  // end the preprocessor directive first.  The next token returned will
  // then be the end of file.
  if (ParsingPreprocessorDirective) {
    ParsingPreprocessorDirective = false; // Done parsing the "line".
    return true;                          // Have a token.
  }

  assert(!LexingRawMode);

  // If we are in a #if directive, emit an error.
  while (!ConditionalStack.empty()) {
    if (PP->getCodeCompletionFileLoc() != FileStartLoc)
      PP->Diag(ConditionalStack.back().IfLoc,
               diag::err_pp_unterminated_conditional);
    ConditionalStack.pop_back();
  }

  // Finally, let the preprocessor handle this.
  //
  // FIXME: Pass a valid EndLoc to fix #pragma clang include_instead.
  return PP->HandleEndOfFile(Result, /*EndLoc=*/{});
}

// FIXME: We can just grab the last token instead of storing a copy
// into EofToken.
void PTHLexer::getEOF(Token &Tok) {
  assert(EofToken.is(tok::eof));
  Tok = EofToken;
}

void PTHLexer::DiscardToEndOfLine() {
  assert(ParsingPreprocessorDirective && ParsingFilename == false &&
         "Must be in a preprocessing directive!");

  // We assume that if the preprocessor wishes to discard to the end of
  // the line that it also means to end the current preprocessor directive.
  ParsingPreprocessorDirective = false;

  // Skip tokens by only peeking at their token kind and the flags.
  // We don't need to actually reconstruct full tokens from the token buffer.
  // This saves some copies and it also reduces IdentifierInfo* lookup.
  const unsigned char *p = CurPtr;
  while (true) {
    // Read the token kind.  Are we at the end of the file?
    const unsigned char *T = p;
    tok::TokenKind x =
        (tok::TokenKind)endian::readNext<uint16_t, little, aligned>(T);
    if (x == tok::eof)
      break;

    // Read the token flags.  Are we at the start of the next line?
    Token::TokenFlags y =
        (Token::TokenFlags)endian::readNext<uint16_t, little, aligned>(T);
    if (y & Token::StartOfLine)
      break;

    // Skip to the next token.
    p += StoredTokenSize;
  }

  CurPtr = p;
}

/// SkipBlock - Used by Preprocessor to skip the current conditional block.
bool PTHLexer::SkipBlock() {
  using namespace llvm::support;

  assert(CurPPCondPtr && "No cached PP conditional information.");
  assert(LastHashTokPtr && "No known '#' token.");

  const unsigned char *HashEntryI = nullptr;
  uint32_t TableIdx;

  do {
    // Read the token offset from the side-table.
    uint32_t Offset = endian::readNext<uint32_t, little, aligned>(CurPPCondPtr);

    // Read the target table index from the side-table.
    TableIdx = endian::readNext<uint32_t, little, aligned>(CurPPCondPtr);

    // Compute the actual memory address of the '#' token data for this entry.
    HashEntryI = TokBuf + Offset;

    // Optimization: "Sibling jumping".  #if...#else...#endif blocks can
    //  contain nested blocks.  In the side-table we can jump over these
    //  nested blocks instead of doing a linear search if the next "sibling"
    //  entry is not at a location greater than LastHashTokPtr.
    if (HashEntryI < LastHashTokPtr && TableIdx) {
      // In the side-table we are still at an entry for a '#' token that
      // is earlier than the last one we saw.  Check if the location we would
      // stride gets us closer.
      const unsigned char *NextPPCondPtr =
          PPCond + TableIdx * (sizeof(uint32_t) * 2);
      assert(NextPPCondPtr >= CurPPCondPtr);
      // Read where we should jump to.
      const unsigned char *HashEntryJ =
          TokBuf + endian::readNext<uint32_t, little, aligned>(NextPPCondPtr);

      if (HashEntryJ <= LastHashTokPtr) {
        // Jump directly to the next entry in the side table.
        HashEntryI = HashEntryJ;
        TableIdx = endian::readNext<uint32_t, little, aligned>(NextPPCondPtr);
        CurPPCondPtr = NextPPCondPtr;
      }
    }
  } while (HashEntryI < LastHashTokPtr);
  assert(HashEntryI == LastHashTokPtr && "No PP-cond entry found for '#'");
  assert(TableIdx && "No jumping from #endifs.");

  // Update our side-table iterator.
  const unsigned char *NextPPCondPtr =
      PPCond + TableIdx * (sizeof(uint32_t) * 2);
  assert(NextPPCondPtr >= CurPPCondPtr);
  CurPPCondPtr = NextPPCondPtr;

  // Read where we should jump to.
  HashEntryI =
      TokBuf + endian::readNext<uint32_t, little, aligned>(NextPPCondPtr);
  uint32_t NextIdx = endian::readNext<uint32_t, little, aligned>(NextPPCondPtr);

  // By construction NextIdx will be zero if this is a #endif.  This is useful
  // to know to obviate lexing another token.
  bool isEndif = NextIdx == 0;

  // This case can occur when we see something like this:
  //
  //  #if ...
  //   /* a comment or nothing */
  //  #elif
  //
  // If we are skipping the first #if block it will be the case that CurPtr
  // already points 'elif'.  Just return.

  if (CurPtr > HashEntryI) {
    assert(CurPtr == HashEntryI + StoredTokenSize);
    // Did we reach a #endif?  If so, go ahead and consume that token as well.
    if (isEndif)
      CurPtr += StoredTokenSize * 2;
    else
      LastHashTokPtr = HashEntryI;

    return isEndif;
  }

  // Otherwise, we need to advance.  Update CurPtr to point to the '#' token.
  CurPtr = HashEntryI;

  // Update the location of the last observed '#'.  This is useful if we
  // are skipping multiple blocks.
  LastHashTokPtr = CurPtr;

  // Skip the '#' token.
  assert(((tok::TokenKind)*CurPtr) == tok::hash);
  CurPtr += StoredTokenSize;

  // Did we reach a #endif?  If so, go ahead and consume that token as well.
  if (isEndif) {
    CurPtr += StoredTokenSize * 2;
  }

  return isEndif;
}

SourceLocation PTHLexer::getSourceLocation() {
  // getSourceLocation is not on the hot path.  It is used to get the location
  // of the next token when transitioning back to this lexer when done
  // handling a #included file.  Just read the necessary data from the token
  // data buffer to construct the SourceLocation object.
  // NOTE: This is a virtual function; hence it is defined out-of-line.
  using namespace llvm::support;

  const unsigned char *OffsetPtr = CurPtr + (StoredTokenSize - 4);
  uint32_t Offset = endian::readNext<uint32_t, little, aligned>(OffsetPtr);
  return FileStartLoc.getLocWithOffset(Offset);
}

//===----------------------------------------------------------------------===//
// PTHManager methods.
//===----------------------------------------------------------------------===//

PTHManager::PTHManager(IntrusiveRefCntPtr<llvm::cas::CASFileSystemBase> FS,
                       Preprocessor &PP)
    : CAS(FS->getCAS()), FS(std::move(FS)), PP(&PP) {
  {
    const LangOptions &OriginalLangOpts = PP.getLangOpts();
    // Copy over most options.
    //
    // FIXME: Drop almost all of these, since we're just doing raw-tokenization.
#define LANGOPT(Name, Bits, Default, Description)                              \
  CanonicalLangOpts.Name = OriginalLangOpts.Name;
#define ENUM_LANGOPT(Name, Type, Bits, Default, Description)                   \
  CanonicalLangOpts.set##Name(OriginalLangOpts.get##Name());
    // Just use the defaults for BENIGN_ options.
#define BENIGN_LANGOPT(Name, Bits, Default, Description)
#define BENIGN_ENUM_LANGOPT(Name, Type, Bits, Default, Description)
#include "clang/Basic/LangOptions.def"
    // Catch a few others.
    //
    // FIXME: Should be formalized somewhere.
    CanonicalLangOpts.LangStd = OriginalLangOpts.LangStd;
    CanonicalLangOpts.ObjCRuntime = OriginalLangOpts.ObjCRuntime;
    CanonicalLangOpts.CFRuntime = OriginalLangOpts.CFRuntime;
  }

  // Serialize.
  //
  // FIXME: Same options as above. Could use command-line generation but that
  // seems like overkill, and it probably doesn't make sense to use a triple
  // here.
  SmallString<256> LangBlock;
  {
    llvm::raw_svector_ostream OS(LangBlock);
    OS << CanonicalLangOpts.LangStd << "\n";
    OS << CanonicalLangOpts.ObjCRuntime << "\n";
    OS << (uint64_t)CanonicalLangOpts.CFRuntime << "\n";
#define LANGOPT(Name, Bits, Default, Description)                              \
  OS << CanonicalLangOpts.Name << "\n";
#define ENUM_LANGOPT(Name, Type, Bits, Default, Description)                   \
  OS << (uint64_t)CanonicalLangOpts.get##Name() << "\n";
    // Skip the benign options since they're set to the defaults.
#define BENIGN_LANGOPT(Name, Bits, Default, Description)
#define BENIGN_ENUM_LANGOPT(Name, Type, Bits, Default, Description)
#include "clang/Basic/LangOptions.def"
  }

  SerializedLangOpts = llvm::cantFail(CAS.createBlob(LangBlock));
}

PTHManager::~PTHManager() = default;

static void InvalidPTH(DiagnosticsEngine &Diags, const char *Msg) {
  Diags.Report(Diags.getCustomDiagID(DiagnosticsEngine::Error, "%0")) << Msg;
}

PTHHandler::PTHHandler(PTHManager &PTHM, DiagnosticsEngine &Diags,
                       StringRef File, StringRef PTH)
    : PTH(PTH) {
  initialize(PTHM, Diags, File);
}

void PTHHandler::initialize(PTHManager &PTHM, DiagnosticsEngine &Diags,
                            StringRef File) {
  // Get the buffer ranges and check if there are at least three 32-bit
  // words at the end of the file.
  const unsigned char *BufBeg = (const unsigned char *)PTH->begin();
  const unsigned char *BufEnd = (const unsigned char *)PTH->end();

  // Check the prologue of the file.
  if ((BufEnd - BufBeg) < (signed)(sizeof("cfe-pth") + 4 + 4) ||
      memcmp(BufBeg, "cfe-pth", sizeof("cfe-pth")) != 0) {
    Diags.Report(diag::err_invalid_pth_file) << File;
    PTH = None;
    return;
  }

  // Read the PTH version.
  const unsigned char *CurrentPtr = BufBeg + sizeof("cfe-pth");
  unsigned Version = endian::readNext<uint32_t, little, aligned>(CurrentPtr);

  if (Version < PTHManager::Version) {
    // FIXME: Should be impossible, due to result caching system?
    InvalidPTH(
        Diags,
        Version < PTHManager::Version
            ? "PTH file uses an older PTH format that is no longer supported"
            : "PTH file uses a newer PTH format that cannot be read");
    PTH = None;
    return;
  }

  // Get the location of the table mapping from persistent ids to the
  // data needed to reconstruct identifiers.
  IdentifierTable =
      BufBeg + endian::readNext<uint32_t, little, aligned>(CurrentPtr);
  if (!(IdentifierTable >= BufBeg && IdentifierTable < BufEnd)) {
    Diags.Report(diag::err_invalid_pth_file) << File;
    PTH = None;
    return;
  }

  // Get the number of IdentifierInfos and pre-allocate the identifier cache.
  if (uint32_t NumIDs =
          endian::read<uint32_t, little, aligned>(IdentifierTable))
    IdentifierInfoCache = llvm::makeMutableArrayRef(
        new (PTHM.IdentifierInfoCacheAlloc.Allocate(NumIDs))
            IdentifierInfo *[NumIDs],
        NumIDs);
  std::fill(IdentifierInfoCache.begin(), IdentifierInfoCache.end(), nullptr);

  // Get the location of the strings table. Note: it might have no content at
  // all and be equal to BufEnd.
  StringsTable =
      BufBeg + endian::readNext<uint32_t, little, aligned>(CurrentPtr);
  if (!(StringsTable >= BufBeg && StringsTable <= BufEnd)) {
    Diags.Report(diag::err_invalid_pth_file) << File;
    PTH = None;
    return;
  }

  // Get the location of the preprocessor conditional table.
  PPCondTable =
      BufBeg + endian::readNext<uint32_t, little, aligned>(CurrentPtr);
  if (!(PPCondTable >= BufBeg && PPCondTable < BufEnd)) {
    Diags.Report(diag::err_invalid_pth_file) << File;
    PTH = None;
    return;
  }
  unsigned PPCondTableSize =
      endian::readNext<uint32_t, little, aligned>(PPCondTable);
  const unsigned char *PPCondTableEnd = PPCondTable + PPCondTableSize;
  if (!PPCondTableSize) {
    PPCondTable = nullptr;
  } else if (!(PPCondTableEnd >= BufBeg && PPCondTableEnd < BufEnd)) {
    Diags.Report(diag::err_invalid_pth_file) << File;
    PTH = None;
    return;
  }

  TokenStream = CurrentPtr;
}

std::unique_ptr<PTHLexer> PTHHandler::createLexer(PTHManager &PTHM,
                                                  FileID FID) {
  if (!PTH)
    return nullptr;

  // Not using std::make_unique because it doesn't have access.
  return std::unique_ptr<PTHLexer>(new PTHLexer(*this, *PTHM.PP, FID));
}

PTHHandler *PTHManager::createHandler(StringRef Filename,
                                      llvm::cas::CASID PTH) {
  if (auto ExpectedBuffer = CAS.getBlob(PTH)) {
    return new (HandlerAlloc.Allocate())
        PTHHandler(*this, PP->getDiagnostics(), Filename, ExpectedBuffer->getData());
  } else {
    // FIXME: Diagnose instead of reporting fatal.
    llvm::report_fatal_error(ExpectedBuffer.takeError());
  }
  return nullptr;
}

std::unique_ptr<PTHLexer> PTHManager::createLexer(FileID FID) {
  const FileEntry *FE = PP->getSourceManager().getFileEntryForID(FID);
  if (!FE)
    return nullptr;

  // Check if we already loaded this PTH.
  auto &Handler = Handlers[FE];
  if (Handler)
    return Handler->createLexer(*this, FID);

  // Create a handler for this PTH.
  //
  // FIXME: Consider reporting diag::err_invalid_pth_file or similar?
  StringRef Filename = FE->getName();
  if (Optional<llvm::cas::CASID> InputFile = FS->getFileCASID(Filename))
    if (Optional<llvm::cas::CASID> PTHFile =
            expectedToOptional(computePTH(*InputFile)))
      Handler = createHandler(Filename, *PTHFile);

  if (!Handler)
    Handler = &NullHandler;

  return Handler->createLexer(*this, FID);
}

namespace {
typedef uint32_t OffsetType;
class PTHWriter {
  typedef llvm::StringMap<Optional<OffsetType>, llvm::BumpPtrAllocator>
      CachedStrsTy;

  raw_pwrite_stream &OS;
  SmallVector<llvm::StringMapEntry<Optional<OffsetType>> *> StrEntries;
  CachedStrsTy CachedStrs;
  OffsetType CurStrOffset = 0;
  llvm::StringMap<uint32_t> RawIDIndexPlus1;
  SmallVector<OffsetType> IDStrings;

  OffsetType getStringOffset(StringRef S);
  OffsetType getLiteralSpellingOffset(const Token &T);
  uint32_t getIDIndex(const Token &T);

  /// Emit a token to the PTH file.
  void EmitToken(const Token &T);

  void Emit8(uint32_t V) { OS << char(V); }

  void Emit16(uint32_t V) {
    using namespace llvm::support;
    endian::write<uint16_t>(OS, V, little);
  }

  void Emit32(uint32_t V) {
    using namespace llvm::support;
    endian::write<uint32_t>(OS, V, little);
  }

  void EmitBuf(const char *Ptr, unsigned NumBytes) { OS.write(Ptr, NumBytes); }

  void EmitString(StringRef V) {
    using namespace llvm::support;
    endian::write<uint16_t>(OS, V.size(), little);
    EmitBuf(V.data(), V.size());
  }

  /// Emits table mapping from persistent IDs to string data.
  OffsetType emitIdentifierTable();

  /// Emits all the tokens, returning the offset to the pp-conditional side
  /// table.
  OffsetType LexTokens(Lexer &L);
  OffsetType emitStringTable();

public:
  PTHWriter(raw_pwrite_stream &OS) : OS(OS) {}

  void generatePTH(llvm::raw_pwrite_stream &OS, StringRef Input,
                   const LangOptions &CanonicalLangOpts);
};
} // end anonymous namespace

Expected<llvm::cas::CASID> PTHManager::computePTH(llvm::cas::CASID InputFile) {
  if (!ClangVersion) {
    assert(!Operation);

    // FIXME: This should be the clang executable...
    ClangVersion = llvm::cantFail(CAS.createBlob(getClangFullVersion()));
    Operation = llvm::cantFail(CAS.createBlob("generate-isolated-pth"));
  }

  llvm::cas::HierarchicalTreeBuilder Builder;
  Builder.push(InputFile, llvm::cas::TreeEntry::Regular, "data");
  Builder.push(*ClangVersion, llvm::cas::TreeEntry::Regular, "version");
  Builder.push(*Operation, llvm::cas::TreeEntry::Regular, "operation");
  Builder.push(*SerializedLangOpts, llvm::cas::TreeEntry::Regular, "lang-opts");

  llvm::cas::CASID CacheKey = llvm::cantFail(Builder.create(CAS)).getID();
  if (Optional<llvm::cas::CASID> CachedPTH =
          expectedToOptional(CAS.getCachedResult(CacheKey)))
    return *CachedPTH;

  SmallString<256> PTHString;
  {
    llvm::raw_svector_ostream OS(PTHString);
    PTHWriter Writer(OS);

    // FIXME: Maybe we should allow this to fail? Then we should cache the
    // failure.
    Writer.generatePTH(OS, llvm::cantFail(CAS.getBlob(InputFile)).getData(),
                       CanonicalLangOpts);
  }
  llvm::cas::BlobRef PTH = llvm::cantFail(CAS.createBlob(PTHString));
  llvm::cantFail(CAS.putCachedResult(CacheKey, PTH));
  return PTH;
}

OffsetType PTHWriter::getStringOffset(StringRef S) {
  auto &E = *CachedStrs.insert(std::make_pair(S, None)).first;

  // If this is a new string entry, bump the PTH offset.
  if (!E.second) {
    E.second = CurStrOffset;
    StrEntries.push_back(&E);
    CurStrOffset += S.size() + 1;
  }

  // Emit the relative offset into the PTH file for the spelling string.
  return *E.second;
}

OffsetType PTHWriter::getLiteralSpellingOffset(const Token &T) {
  // We cache *un-cleaned* spellings. This gives us 100% fidelity with the
  // source code.
  return getStringOffset(StringRef(T.getLiteralData(), T.getLength()));
}

uint32_t PTHWriter::getIDIndex(const Token &T) {
  assert(T.isAnyIdentifier());
  assert(T.is(tok::raw_identifier));
  StringRef ID = T.getRawIdentifier();
  auto &IndexPlus1 = RawIDIndexPlus1[ID];
  if (!IndexPlus1) {
    IDStrings.push_back(getStringOffset(ID));
    IndexPlus1 = IDStrings.size();
  }
  return IndexPlus1 - 1;
}

void PTHWriter::EmitToken(const Token &T) {
  // Emit the token kind, flags, and length.
  Emit16((uint16_t)T.getKind());
  Emit16((uint16_t)T.getFlags());
  Emit32((uint32_t)T.getLength());

  // Check the sizes. Be sure to update the reader too if this changes.
  static_assert(sizeof(T.getKind()) <= 2, "Kind too big");
  // FIXME: The underlying field is 2 bytes, but we can't check that here:
  //
  // static_assert(sizeof(T.getFlags()) <= 2, "Flags too big");
  static_assert(sizeof(T.getLength()) <= 4, "Length too big");

  if (T.isAnyIdentifier())
    Emit32(getIDIndex(T));
  else if (T.isLiteral())
    Emit32(getLiteralSpellingOffset(T));
  else
    Emit32(0);

  // Emit the offset into the original source file of this token so that we
  // can reconstruct its SourceLocation.
  //
  // FIXME: Should be a better way to do this.
  assert(T.getLocation().isFileID() &&
         "Expected this to be a raw lex without macro expansion");
  Emit32(SourceManager::getLocationOffset(T.getLocation()));
}

namespace clang {
tok::PPKeywordKind getIdentifierInfoPPKeywordID(StringRef Spelling);
}

OffsetType PTHWriter::LexTokens(Lexer &L) {
  // Pad 0's so that we emit tokens to a 4-byte alignment.
  // This speed up reading them back in.
  using namespace llvm::support;
  endian::Writer LE(OS, little);
  uint32_t TokenOff = OS.tell();
  for (uint64_t N = llvm::offsetToAlignment(TokenOff, llvm::Align(4)); N; --N, ++TokenOff)
    LE.write<uint8_t>(0);

  // Keep track of matching '#if' ... '#endif'.
  typedef std::vector<std::pair<OffsetType, unsigned>> PPCondTable;
  PPCondTable PPCond;
  std::vector<unsigned> PPStartCond;
  bool ParsingPreprocessorDirective = false;
  Token Tok;

  do {
    L.LexFromRawLexer(Tok);
NextToken:

    if ((Tok.isAtStartOfLine() || Tok.is(tok::eof)) &&
        ParsingPreprocessorDirective) {
      // Insert an eod token into the token cache.  It has the same
      // position as the next token that is not on the same line as the
      // preprocessor directive.  Observe that we continue processing
      // 'Tok' when we exit this branch.
      Token Tmp = Tok;
      Tmp.setKind(tok::eod);
      Tmp.clearFlag(Token::StartOfLine);
      Tmp.setIdentifierInfo(nullptr);
      EmitToken(Tmp);
      ParsingPreprocessorDirective = false;
    }

    if (Tok.is(tok::raw_identifier)) {
      // FIXME: should we in fact do this when lexing? For now, not doing it
      // since it seems less likely to be profitible when there are so many PTH
      // files.
      //
      // PP.LookUpIdentifierInfo(Tok);
      EmitToken(Tok);
      continue;
    }

    // Simple token. Just continue.
    if (!Tok.is(tok::hash) || !Tok.isAtStartOfLine()) {
      EmitToken(Tok);
      continue;
    }

    // Special processing for #include.  Store the '#' token and lex
    // the next token.
    assert(!ParsingPreprocessorDirective);
    OffsetType HashOff = (OffsetType)OS.tell();

    // Get the next token.
    Token NextTok;
    L.LexFromRawLexer(NextTok);

    // If we see the start of line, then we had a null directive "#".  In
    // this case, discard both tokens.
    if (NextTok.isAtStartOfLine())
      goto NextToken;

    // The token is the start of a directive.  Emit it.
    EmitToken(Tok);
    Tok = NextTok;

    // Did we see 'include'/'import'/'include_next'?
    if (Tok.isNot(tok::raw_identifier)) {
      EmitToken(Tok);
      continue;
    }

    // FIXME: This is effectively a call to IdentifierInfo::getPPKeywordID().
    // Note that it relies on #if being null-terminated.
    tok::PPKeywordKind K =
        Tok.getRawIdentifier() == "if"
            ? tok::pp_if
            : clang::getIdentifierInfoPPKeywordID(Tok.getRawIdentifier());

    ParsingPreprocessorDirective = true;

    // FIXME: The following state machine assumes that the #if/etc. logic is
    // balanced, but we haven't checked that yet. We should abort the state
    // machine if there's a problem, rather than asserting and blindly
    // continuing.
    SmallString<128> Message;
    switch (K) {
    case tok::pp_error:
    case tok::pp_warning: {
      EmitToken(Tok);

      // FIXME: This isn't really a string literal, but that's an easy way to
      // pass the data through.
      Message.clear();
      L.ReadToEndOfLine(&Message);
      (void)Message.c_str(); // Null-terminate.
      StringRef Trimmed = Message;
      Trimmed = Trimmed.ltrim();
      Tok.setKind(tok::string_literal);
      Tok.setLiteralData(Trimmed.begin());
      EmitToken(Tok);
      continue;
    }
    case tok::pp_not_keyword:
      // Invalid directives "#foo" can occur in #if 0 blocks etc, just pass
      // them through.
    default:
      break;

    case tok::pp_include:
    case tok::pp_import:
    case tok::pp_include_next: {
      // Save the 'include' token.
      EmitToken(Tok);
      // Lex the next token as an include string.
      L.setParsingPreprocessorDirective(true);
      L.LexIncludeFilename(Tok);
      L.setParsingPreprocessorDirective(false);
      assert(!Tok.isAtStartOfLine());
      // if (Tok.is(tok::raw_identifier))
      //   PP.LookUpIdentifierInfo(Tok);

      break;
    }
    case tok::pp_if:
    case tok::pp_ifdef:
    case tok::pp_ifndef: {
      // Add an entry for '#if' and friends.  We initially set the target
      // index to 0.  This will get backpatched when we hit #endif.
      PPStartCond.push_back(PPCond.size());
      PPCond.push_back(std::make_pair(HashOff, 0U));
      break;
    }
    case tok::pp_endif: {
      // Add an entry for '#endif'.  We set the target table index to itself.
      // This will later be set to zero when emitting to the PTH file.  We
      // use 0 for uninitialized indices because that is easier to debug.
      unsigned index = PPCond.size();
      // Backpatch the opening '#if' entry.
      assert(!PPStartCond.empty());
      assert(PPCond.size() > PPStartCond.back());
      assert(PPCond[PPStartCond.back()].second == 0);
      PPCond[PPStartCond.back()].second = index;
      PPStartCond.pop_back();
      // Add the new entry to PPCond.
      PPCond.push_back(std::make_pair(HashOff, index));
      EmitToken(Tok);

      // Some files have gibberish on the same line as '#endif'.
      // Discard these tokens.
      do
        L.LexFromRawLexer(Tok);
      while (Tok.isNot(tok::eof) && !Tok.isAtStartOfLine());
      // We have the next token in hand.
      // Don't immediately lex the next one.
      goto NextToken;
    }
    case tok::pp_elif:
    case tok::pp_else: {
      // Add an entry for #elif or #else.
      // This serves as both a closing and opening of a conditional block.
      // This means that its entry will get backpatched later.
      unsigned index = PPCond.size();
      // Backpatch the previous '#if' entry.
      assert(!PPStartCond.empty());
      assert(PPCond.size() > PPStartCond.back());
      assert(PPCond[PPStartCond.back()].second == 0);
      PPCond[PPStartCond.back()].second = index;
      PPStartCond.pop_back();
      // Now add '#elif' as a new block opening.
      PPCond.push_back(std::make_pair(HashOff, 0U));
      PPStartCond.push_back(index);
      break;
    }
    }

    EmitToken(Tok);
  } while (Tok.isNot(tok::eof));

  assert(PPStartCond.empty() && "Error: imblanced preprocessor conditionals.");

  // Next write out PPCond.
  OffsetType PPCondOff = (OffsetType)OS.tell();

  // Write out the size of PPCond so that clients can identifer empty tables.
  Emit32(PPCond.size());

  for (unsigned i = 0, e = PPCond.size(); i != e; ++i) {
    Emit32(PPCond[i].first - TokenOff);
    uint32_t x = PPCond[i].second;
    assert(x != 0 && "PPCond entry not backpatched.");
    // Emit zero for #endifs.  This allows us to do checking when
    // we read the PTH file back in.
    Emit32(x == i ? 0 : x);
  }

  return PPCondOff;
}

OffsetType PTHWriter::emitStringTable() {
  // Write each cached strings to the PTH file.
  OffsetType SpellingsOff = OS.tell();

  for (auto &Entry : StrEntries)
    EmitBuf(Entry->getKeyData(), Entry->getKeyLength() + 1 /*null-terminated*/);

  return SpellingsOff;
}

static uint32_t swap32le(uint32_t X) {
  return llvm::support::endian::byte_swap<uint32_t, llvm::support::little>(X);
}

static void pwrite32le(raw_pwrite_stream &OS, uint32_t Val, uint64_t Off) {
  uint32_t LEVal = swap32le(Val);
  OS.pwrite(reinterpret_cast<const char *>(&LEVal), 4, Off);
}

void PTHWriter::generatePTH(llvm::raw_pwrite_stream &OS, StringRef Input,
                            const LangOptions &CanonicalLangOpts) {
  // Generate the prologue.
  OS << "cfe-pth" << '\0';
  Emit32(PTHManager::Version);

  // Leave space for the prologue.
  OffsetType ReservedForIDTable = OS.tell();
  Emit32(0);
  OffsetType ReservedForStringTable = OS.tell();
  Emit32(0);
  OffsetType ReservedForPPCond = OS.tell();
  Emit32(0);

  // Create a lexer cache the tokens.
  Lexer L(SourceLocation(), CanonicalLangOpts, Input.begin(), Input.begin(),
          Input.end());
  OffsetType PPCondOff = LexTokens(L);

  // Write out the identifier table.
  OffsetType IdTableOff = emitIdentifierTable();

  // Write out the cached strings table.
  OffsetType SpellingOff = emitStringTable();

  // Finally, write the prologue.
  pwrite32le(OS, IdTableOff, ReservedForIDTable);
  pwrite32le(OS, SpellingOff, ReservedForStringTable);
  pwrite32le(OS, PPCondOff, ReservedForPPCond);
}

OffsetType PTHWriter::emitIdentifierTable() {
  // Now emit the table mapping from persistent IDs to PTH file offsets.
  OffsetType TableOffset = OS.tell();
  Emit32(IDStrings.size()); // Emit the number of identifiers.
  for (OffsetType Offset : IDStrings)
    Emit32(Offset);

  return TableOffset;
}
