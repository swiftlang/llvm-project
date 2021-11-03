//===- llvm/TableGen/ScanDependencies.h - TableGen scan-deps ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TABLEGEN_SCANDEPENDENCIES_H
#define LLVM_TABLEGEN_SCANDEPENDENCIES_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/Support/Error.h"

namespace llvm {

namespace cas {
class CachingOnDiskFileSystem;
} // end namespace cas

class TreePathPrefixMapper;
struct MappedPrefix;

namespace tablegen {

/// FIXME: Maybe this and \a flattenStrings() could/should go somewhere in
/// Support? Although it's a pretty simple serialization, not hard to rewrite.
void splitFlattenedStrings(StringRef Flattened,
                           SmallVectorImpl<StringRef> &List);
void flattenStrings(ArrayRef<StringRef> List, SmallVectorImpl<char> &Flattened);

/// Helper for fuzzy-matching include directives in tablegen files. This is a
/// very lazy lex, matching 'include "..."' even in comments. It does not
/// descend into the files.
void scanTextForIncludes(StringRef Input, SmallVectorImpl<StringRef> &Includes);

/// Cached version of the above.
///
/// \p ExecID is the Blob for the running executable. It can be used as part of
/// the key for caching to avoid (fixed) bugs poisoning results.
Error scanTextForIncludes(cas::CASDB &CAS, cas::CASID ExecID,
                          const cas::BlobRef &Blob,
                          SmallVectorImpl<StringRef> &Includes);

/// Match logic from SourceMgr::AddIncludeFile.
///
/// FIXME: Take CASFileSystemBase once it adds statusAndFileID().
Optional<cas::CASID> lookupIncludeID(cas::CachingOnDiskFileSystem &FS,
                                     ArrayRef<std::string> IncludeDirs,
                                     StringRef Filename);

/// Helper to access all includes under \p MainFileBlob.
///
/// \p ExecID is the Blob for the running executable. It can be used as part of
/// the key for caching to avoid (fixed) bugs poisoning results.
Error accessAllIncludes(cas::CachingOnDiskFileSystem &FS, cas::CASID ExecID,
                        ArrayRef<std::string> IncludeDirs,
                        const cas::BlobRef &MainFileBlob);

Error createMainFileError(StringRef MainFilename, std::error_code EC);

struct ScanIncludesResult {
  cas::CASID IncludesTree;
  cas::BlobRef MainBlob;
};

/// Scan includes and build a CAS tree. If \p PrefixMappings is not \c None,
/// remap the includes while building the tree. If \p PM is not \c nullptr
/// then the \a TreePathPrefixMapper used will be returned there.
///
/// \p ExecID is the Blob for the running executable. It can be used as part of
/// the key for caching to avoid (fixed) bugs poisoning results.
///
/// TODO: We should upstream a version of this that doesn't rely on the CAS;
/// it'd be useful for build systems that want to collect dependencies ahead of
/// time. Maybe can separate a tool/etc. for that.
Expected<ScanIncludesResult>
scanIncludes(cas::CASDB &CAS, cas::CASID ExecID, StringRef MainFilename,
             ArrayRef<std::string> IncludeDirs,
             ArrayRef<MappedPrefix> PrefixMappings = None,
             Optional<TreePathPrefixMapper> *CapturedPM = nullptr);

/// Scan includes and build a CAS tree where their prefixes have been remapped
/// according to \a PrefixMappings. Also remap the main file and include
/// directories. Return the includes tree and the main file blob.
///
/// \p ExecID is the Blob for the running executable. It can be used as part of
/// the key for caching to avoid (fixed) bugs poisoning results.
Expected<ScanIncludesResult>
scanIncludesAndRemap(cas::CASDB &CAS, cas::CASID ExecID,
                     std::string &MainFilename,
                     std::vector<std::string> &IncludeDirs,
                     ArrayRef<MappedPrefix> PrefixMappings);

} // end namespace tablegen
} // end namespace llvm

#endif // LLVM_TABLEGEN_SCANDEPENDENCIES_H
