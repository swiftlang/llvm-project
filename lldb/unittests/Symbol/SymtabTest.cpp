//===-- SymbolTest.cpp ----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/ObjectFile/ELF/ObjectFileELF.h"
#include "Plugins/ObjectFile/Mach-O/ObjectFileMachO.h"
#include "Plugins/SymbolFile/DWARF/SymbolFileDWARF.h"
#include "Plugins/SymbolFile/DWARF/SymbolFileDWARFDebugMap.h"
#include "Plugins/SymbolFile/Symtab/SymbolFileSymtab.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "TestingSupport/SubsystemRAII.h"
#include "TestingSupport/TestUtilities.h"

#include "lldb/Core/DataFileCache.h"
#include "lldb/Core/Module.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/Symtab.h"
#include "lldb/Utility/DataBuffer.h"
#include "lldb/Utility/DataEncoder.h"
#include "lldb/Utility/DataExtractor.h"

#include "llvm/ADT/ScopeExit.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

#include <memory>

#include "gtest/gtest.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::plugin::dwarf;

class SymtabTest : public testing::Test {
  SubsystemRAII<FileSystem, HostInfo, ObjectFileMachO, SymbolFileDWARF,
                ObjectFileELF, SymbolFileSymtab, TypeSystemClang>
      subsystem;
};

TEST_F(SymtabTest, ExternalASTFilesAreDeduplicatedPerConsumer) {
  auto object = TestFile::fromYaml(R"(
--- !mach-o
FileHeader:
  magic:      0xFEEDFACF
  cputype:    0x100000C
  cpusubtype: 0x0
  filetype:   0x2
  ncmds:      0
  sizeofcmds: 0
  flags:      0x0
  reserved:   0x0
)");
  ASSERT_THAT_EXPECTED(object, llvm::Succeeded());

  llvm::SmallString<128> directory;
  ASSERT_NO_ERROR(
      llvm::sys::fs::createUniqueDirectory("lldb-ast-files", directory));
  auto cleanup =
      llvm::scope_exit([&] { llvm::sys::fs::remove_directories(directory); });
  // Keep absolute paths and getcwd spellings consistent on hosts where the
  // temporary directory has a symlinked parent (for example /var on macOS).
  llvm::SmallString<128> real_directory;
  ASSERT_NO_ERROR(llvm::sys::fs::real_path(directory, real_directory));
  directory = real_directory;
  auto file_path = [&](llvm::StringRef parent) {
    llvm::SmallString<128> path(directory);
    llvm::sys::path::append(path, parent, "Shared.swiftmodule");
    return path;
  };
  auto write_file = [&](llvm::StringRef path, llvm::StringRef contents) {
    ASSERT_NO_ERROR(
        llvm::sys::fs::create_directories(llvm::sys::path::parent_path(path)));
    std::error_code error;
    llvm::raw_fd_ostream stream(path, error);
    ASSERT_NO_ERROR(error);
    stream << contents;
  };
  auto make_image = [&](llvm::StringRef path) -> ModuleSP {
    auto module_spec = object->moduleSpec();
    module_spec.GetFileSpec() =
        FileSpec(directory).CopyByAppendingPathComponent("image");
    auto module = std::make_shared<Module>(module_spec);
    ObjectFile *file = module->GetObjectFile();
    EXPECT_NE(file, nullptr);
    if (!file)
      return {};
    file->GetSymtab()->AddSymbol(Symbol(0, path, eSymbolTypeASTFile, false,
                                        true, false, false, SectionSP(), 0, 0,
                                        false, false, 0));
    return module;
  };
  auto ast_data = [&](const ModuleSP &module, llvm::StringSet<> *loaded) {
    SymbolFileDWARFDebugMap provider(
        module->GetObjectFile()->shared_from_this());
    return static_cast<SymbolFile &>(provider).GetASTData(eLanguageTypeSwift,
                                                          loaded);
  };
  auto contents = [](const DataBufferSP &buffer) {
    return llvm::StringRef(reinterpret_cast<const char *>(buffer->GetBytes()),
                           buffer->GetByteSize());
  };

  auto first_path = file_path("first");
  auto other_path = file_path("other");
  auto missing_path = file_path("missing");
  write_file(first_path, "first AST");
  write_file(other_path, "different AST with the same filename");
  auto first = make_image(first_path);
  auto duplicate = make_image(first_path);
  auto other = make_image(other_path);
  auto missing = make_image(missing_path);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(duplicate, nullptr);
  ASSERT_NE(other, nullptr);
  ASSERT_NE(missing, nullptr);

  llvm::StringSet<> loaded;
  auto first_buffers = ast_data(first, &loaded);
  ASSERT_EQ(first_buffers.size(), 1u);
  EXPECT_EQ(contents(first_buffers.front()), "first AST");
  EXPECT_TRUE(ast_data(duplicate, &loaded).empty());
  EXPECT_EQ(loaded.size(), 1u);
  // The optional set must not change consumers requesting all buffers.
  EXPECT_EQ(ast_data(duplicate, nullptr).size(), 1u);

  auto other_buffers = ast_data(other, &loaded);
  ASSERT_EQ(other_buffers.size(), 1u);
  EXPECT_EQ(contents(other_buffers.front()),
            "different AST with the same filename");
  EXPECT_EQ(loaded.size(), 2u);

  // Keep the original N_AST order when omitting a repeated external file.
  auto ordered = make_image(other_path);
  ASSERT_NE(ordered, nullptr);
  Symtab *ordered_symbols = ordered->GetObjectFile()->GetSymtab();
  ordered_symbols->AddSymbol(Symbol(1, first_path, eSymbolTypeASTFile, false,
                                    true, false, false, SectionSP(), 0, 0,
                                    false, false, 0));
  ordered_symbols->AddSymbol(Symbol(2, other_path, eSymbolTypeASTFile, false,
                                    true, false, false, SectionSP(), 0, 0,
                                    false, false, 0));
  llvm::StringSet<> ordered_context;
  auto ordered_buffers = ast_data(ordered, &ordered_context);
  ASSERT_EQ(ordered_buffers.size(), 2u);
  EXPECT_EQ(contents(ordered_buffers[0]),
            "different AST with the same filename");
  EXPECT_EQ(contents(ordered_buffers[1]), "first AST");
  EXPECT_EQ(ast_data(ordered, nullptr).size(), 3u);

  // A missing file must remain retryable when a later image needs it.
  EXPECT_TRUE(ast_data(missing, &loaded).empty());
  EXPECT_EQ(loaded.size(), 2u);
  write_file(missing_path, "now available");
  auto late_buffers = ast_data(missing, &loaded);
  ASSERT_EQ(late_buffers.size(), 1u);
  EXPECT_EQ(contents(late_buffers.front()), "now available");

  // A new consumer has no stale path cache after a build replaces a file.
  first_buffers.clear();
  write_file(first_path, "replacement AST");
  llvm::StringSet<> fresh_context;
  auto fresh_buffers = ast_data(first, &fresh_context);
  ASSERT_EQ(fresh_buffers.size(), 1u);
  EXPECT_EQ(contents(fresh_buffers.front()), "replacement AST");

  // Relative paths use the current cwd for each provider call, not a cwd
  // captured for the entire consumer lifetime.
  llvm::SmallString<128> original_cwd;
  ASSERT_NO_ERROR(llvm::sys::fs::current_path(original_cwd));
  auto restore_cwd =
      llvm::scope_exit([&] { llvm::sys::fs::set_current_path(original_cwd); });
  auto relative = make_image("Shared.swiftmodule");
  llvm::StringSet<> relative_context;
  ASSERT_NO_ERROR(llvm::sys::fs::set_current_path(
      llvm::sys::path::parent_path(first_path)));
  auto relative_first = ast_data(relative, &relative_context);
  ASSERT_EQ(relative_first.size(), 1u);
  EXPECT_EQ(contents(relative_first.front()), "replacement AST");
  EXPECT_TRUE(ast_data(relative, &relative_context).empty());
  // Raw-spelling misses still normalize to the successful absolute key.
  auto dot_alias = make_image("./Shared.swiftmodule");
  auto parent_alias = make_image("unused/../Shared.swiftmodule");
  llvm::SmallString<128> absolute_alias(
      llvm::sys::path::parent_path(first_path));
  llvm::sys::path::append(absolute_alias, "unused", "..", "Shared.swiftmodule");
  auto absolute_parent_alias = make_image(absolute_alias);
  EXPECT_TRUE(ast_data(dot_alias, &relative_context).empty());
  EXPECT_TRUE(ast_data(parent_alias, &relative_context).empty());
  EXPECT_TRUE(ast_data(absolute_parent_alias, &relative_context).empty());
  EXPECT_TRUE(ast_data(first, &relative_context).empty());
  EXPECT_EQ(relative_context.size(), 1u);
  ASSERT_NO_ERROR(llvm::sys::fs::set_current_path(
      llvm::sys::path::parent_path(other_path)));
  auto relative_other = ast_data(relative, &relative_context);
  ASSERT_EQ(relative_other.size(), 1u);
  EXPECT_EQ(contents(relative_other.front()),
            "different AST with the same filename");
  EXPECT_EQ(relative_context.size(), 2u);
  EXPECT_TRUE(ast_data(first, &relative_context).empty());

  // A basename first searches cwd, then next to the referring image. A later
  // cwd file must still win over the already-loaded adjacent candidate.
  std::string adjacent_name =
      llvm::sys::path::filename(directory).str() + ".swiftmodule";
  llvm::SmallString<128> adjacent_path(first->GetFileSpec().GetDirectory());
  llvm::sys::path::append(adjacent_path, adjacent_name);
  write_file(adjacent_path, "adjacent AST");
  auto adjacent = make_image(adjacent_name);
  llvm::StringSet<> adjacent_context;
  auto adjacent_buffers = ast_data(adjacent, &adjacent_context);
  ASSERT_EQ(adjacent_buffers.size(), 1u);
  EXPECT_EQ(contents(adjacent_buffers.front()), "adjacent AST");
  llvm::SmallString<128> cwd_path(llvm::sys::path::parent_path(other_path));
  llvm::sys::path::append(cwd_path, adjacent_name);
  write_file(cwd_path, "cwd AST");
  auto cwd_buffers = ast_data(adjacent, &adjacent_context);
  ASSERT_EQ(cwd_buffers.size(), 1u);
  EXPECT_EQ(contents(cwd_buffers.front()), "cwd AST");
  EXPECT_EQ(adjacent_context.size(), 2u);

  auto late_relative = make_image("late/Shared.swiftmodule");
  llvm::StringSet<> late_context;
  EXPECT_TRUE(ast_data(late_relative, &late_context).empty());
  EXPECT_TRUE(late_context.empty());
  llvm::SmallString<128> late_path(llvm::sys::path::parent_path(other_path));
  llvm::sys::path::append(late_path, "late", "Shared.swiftmodule");
  write_file(late_path, "late relative AST");
  auto late_relative_buffers = ast_data(late_relative, &late_context);
  ASSERT_EQ(late_relative_buffers.size(), 1u);
  EXPECT_EQ(contents(late_relative_buffers.front()), "late relative AST");

  // A custom VFS override is not replaced by the native lexical fast path.
  // Its returned path must anchor both the dedup key and the opened buffer.
  class ASTPathFileSystem : public llvm::vfs::ProxyFileSystem {
  public:
    ASTPathFileSystem(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> fs,
                      llvm::StringRef path)
        : ProxyFileSystem(std::move(fs)), path(path) {}
    std::error_code
    makeAbsolute(llvm::SmallVectorImpl<char> &value) const override {
      ++calls;
      if (fail)
        return std::make_error_code(std::errc::permission_denied);
      value.assign(path.begin(), path.end());
      return {};
    }
    std::string path;
    mutable unsigned calls = 0;
    bool fail = false;
  };
  auto original_fs = FileSystem::Instance().GetVirtualFileSystem();
  auto custom_fs =
      llvm::makeIntrusiveRefCnt<ASTPathFileSystem>(original_fs, first_path);
  FileSystem::Terminate();
  FileSystem::Initialize(custom_fs);
  auto restore_fs = llvm::scope_exit([&] {
    FileSystem::Terminate();
    FileSystem::Initialize(original_fs);
  });
  llvm::StringSet<> custom_context;
  auto custom_buffers = ast_data(relative, &custom_context);
  ASSERT_EQ(custom_buffers.size(), 1u);
  EXPECT_EQ(contents(custom_buffers.front()), "replacement AST");
  EXPECT_EQ(custom_fs->calls, 1u);
  EXPECT_TRUE(custom_context.contains(first_path));

  // Even an exact absolute spelling must still honor a custom VFS redirect.
  auto custom_absolute = make_image(first_path);
  custom_fs->path = other_path.str().str();
  auto redirected_buffers = ast_data(custom_absolute, &custom_context);
  ASSERT_EQ(redirected_buffers.size(), 1u);
  EXPECT_EQ(contents(redirected_buffers.front()),
            "different AST with the same filename");
  EXPECT_EQ(custom_fs->calls, 2u);
  EXPECT_EQ(custom_context.size(), 2u);

  // Failed resolution retains legacy relative reads without poisoning the
  // consumer set. Callers without a consumer set still request every buffer.
  custom_fs->fail = true;
  llvm::StringSet<> failed_context;
  auto failed_buffers = ast_data(relative, &failed_context);
  ASSERT_EQ(failed_buffers.size(), 1u);
  EXPECT_EQ(contents(failed_buffers.front()),
            "different AST with the same filename");
  EXPECT_TRUE(failed_context.empty());
  unsigned previous_calls = custom_fs->calls;
  EXPECT_EQ(ast_data(relative, nullptr).size(), 1u);
  EXPECT_EQ(custom_fs->calls, previous_calls);

  // The same consumer retries after failed anchoring, then deduplicates the
  // successful result without retaining a failure or a raw-spelling cache.
  custom_fs->fail = false;
  auto retried_buffers = ast_data(relative, &failed_context);
  ASSERT_EQ(retried_buffers.size(), 1u);
  EXPECT_EQ(contents(retried_buffers.front()),
            "different AST with the same filename");
  EXPECT_TRUE(failed_context.contains(other_path));
  EXPECT_EQ(failed_context.size(), 1u);
  EXPECT_EQ(custom_fs->calls, previous_calls + 1);
  EXPECT_TRUE(ast_data(relative, &failed_context).empty());
  EXPECT_EQ(custom_fs->calls, previous_calls + 2);
}

static void EncodeDecode(const Symtab &object, ByteOrder byte_order) {
  const uint8_t addr_size = 8;
  DataEncoder file(byte_order, addr_size);

  object.Encode(file);
  llvm::ArrayRef<uint8_t> bytes = file.GetData();
  DataExtractor data(bytes.data(), bytes.size(), byte_order, addr_size);
  Symtab decoded_object(object.GetObjectFile());
  offset_t data_offset = 0;
  bool uuid_mismatch = false;
  decoded_object.Decode(data, &data_offset, uuid_mismatch);
  ASSERT_EQ(object.GetNumSymbols(), decoded_object.GetNumSymbols());
  for (size_t i = 0; i < object.GetNumSymbols(); ++i)
    EXPECT_EQ(*object.SymbolAtIndex(i), *decoded_object.SymbolAtIndex(i));
}

static void EncodeDecode(const Symtab &object) {
  EncodeDecode(object, eByteOrderLittle);
  EncodeDecode(object, eByteOrderBig);
}

TEST_F(SymtabTest, EncodeDecodeSymtab) {

  auto ExpectedFile = TestFile::fromYaml(R"(
--- !mach-o
FileHeader:
  magic:           0xFEEDFACF
  cputype:         0x100000C
  cpusubtype:      0x0
  filetype:        0x2
  ncmds:           17
  sizeofcmds:      792
  flags:           0x200085
  reserved:        0x0
LoadCommands:
  - cmd:             LC_SEGMENT_64
    cmdsize:         72
    segname:         __PAGEZERO
    vmaddr:          0
    vmsize:          4294967296
    fileoff:         0
    filesize:        0
    maxprot:         0
    initprot:        0
    nsects:          0
    flags:           0
  - cmd:             LC_SEGMENT_64
    cmdsize:         232
    segname:         __TEXT
    vmaddr:          4294967296
    vmsize:          16384
    fileoff:         0
    filesize:        16384
    maxprot:         5
    initprot:        5
    nsects:          2
    flags:           0
    Sections:
      - sectname:        __text
        segname:         __TEXT
        addr:            0x100003F94
        size:            36
        offset:          0x3F94
        align:           2
        reloff:          0x0
        nreloc:          0
        flags:           0x80000400
        reserved1:       0x0
        reserved2:       0x0
        reserved3:       0x0
        content:         FF8300D1E80300AA00008052FF1F00B9E81B00B9E10B00F9E20700F9FF830091C0035FD6
      - sectname:        __unwind_info
        segname:         __TEXT
        addr:            0x100003FB8
        size:            72
        offset:          0x3FB8
        align:           2
        reloff:          0x0
        nreloc:          0
        flags:           0x0
        reserved1:       0x0
        reserved2:       0x0
        reserved3:       0x0
        content:         010000001C000000000000001C000000000000001C00000002000000943F00003400000034000000B93F00000000000034000000030000000C000100100001000000000000200002
  - cmd:             LC_SEGMENT_64
    cmdsize:         72
    segname:         __LINKEDIT
    vmaddr:          4294983680
    vmsize:          16384
    fileoff:         16384
    filesize:        674
    maxprot:         1
    initprot:        1
    nsects:          0
    flags:           0
  - cmd:             LC_DYLD_CHAINED_FIXUPS
    cmdsize:         16
    dataoff:         16384
    datasize:        56
  - cmd:             LC_DYLD_EXPORTS_TRIE
    cmdsize:         16
    dataoff:         16440
    datasize:        48
  - cmd:             LC_SYMTAB
    cmdsize:         24
    symoff:          16496
    nsyms:           10
    stroff:          16656
    strsize:         128
  - cmd:             LC_DYSYMTAB
    cmdsize:         80
    ilocalsym:       0
    nlocalsym:       8
    iextdefsym:      8
    nextdefsym:      2
    iundefsym:       10
    nundefsym:       0
    tocoff:          0
    ntoc:            0
    modtaboff:       0
    nmodtab:         0
    extrefsymoff:    0
    nextrefsyms:     0
    indirectsymoff:  0
    nindirectsyms:   0
    extreloff:       0
    nextrel:         0
    locreloff:       0
    nlocrel:         0
  - cmd:             LC_LOAD_DYLINKER
    cmdsize:         32
    name:            12
    Content:         '/usr/lib/dyld'
    ZeroPadBytes:    7
  - cmd:             LC_UUID
    cmdsize:         24
    uuid:            1EECD2B8-16EA-3FEC-AB3C-F46139DBD0E2
  - cmd:             LC_BUILD_VERSION
    cmdsize:         32
    platform:        1
    minos:           786432
    sdk:             786432
    ntools:          1
    Tools:
      - tool:            3
        version:         46596096
  - cmd:             LC_SOURCE_VERSION
    cmdsize:         16
    version:         0
  - cmd:             LC_MAIN
    cmdsize:         24
    entryoff:        16276
    stacksize:       0
  - cmd:             LC_LOAD_DYLIB
    cmdsize:         48
    dylib:
      name:            24
      timestamp:       2
      current_version: 78643968
      compatibility_version: 65536
    Content:         '/usr/lib/libc++.1.dylib'
    ZeroPadBytes:    1
  - cmd:             LC_LOAD_DYLIB
    cmdsize:         56
    dylib:
      name:            24
      timestamp:       2
      current_version: 85917696
      compatibility_version: 65536
    Content:         '/usr/lib/libSystem.B.dylib'
    ZeroPadBytes:    6
  - cmd:             LC_FUNCTION_STARTS
    cmdsize:         16
    dataoff:         16488
    datasize:        8
  - cmd:             LC_DATA_IN_CODE
    cmdsize:         16
    dataoff:         16496
    datasize:        0
  - cmd:             LC_CODE_SIGNATURE
    cmdsize:         16
    dataoff:         16784
    datasize:        274
LinkEditData:
  NameList:
    - n_strx:          28
      n_type:          0x64
      n_sect:          0
      n_desc:          0
      n_value:         0
    - n_strx:          64
      n_type:          0x64
      n_sect:          0
      n_desc:          0
      n_value:         0
    - n_strx:          73
      n_type:          0x66
      n_sect:          0
      n_desc:          1
      n_value:         1639532873
    - n_strx:          1
      n_type:          0x2E
      n_sect:          1
      n_desc:          0
      n_value:         4294983572
    - n_strx:          115
      n_type:          0x24
      n_sect:          1
      n_desc:          0
      n_value:         4294983572
    - n_strx:          1
      n_type:          0x24
      n_sect:          0
      n_desc:          0
      n_value:         36
    - n_strx:          1
      n_type:          0x4E
      n_sect:          1
      n_desc:          0
      n_value:         36
    - n_strx:          1
      n_type:          0x64
      n_sect:          1
      n_desc:          0
      n_value:         0
    - n_strx:          2
      n_type:          0xF
      n_sect:          1
      n_desc:          16
      n_value:         4294967296
    - n_strx:          22
      n_type:          0xF
      n_sect:          1
      n_desc:          0
      n_value:         4294983572
  StringTable:
    - ' '
    - __mh_execute_header
    - _main
    - '/Users/gclayton/Documents/src/args/'
    - main.cpp
    - '/Users/gclayton/Documents/src/args/main.o'
    - _main
    - ''
    - ''
    - ''
    - ''
    - ''
    - ''
    - ''
...
)");

  ASSERT_THAT_EXPECTED(ExpectedFile, llvm::Succeeded());
  auto module_sp = std::make_shared<Module>(ExpectedFile->moduleSpec());
  ObjectFile *objfile = module_sp->GetObjectFile();
  ASSERT_NE(objfile, nullptr);

  // Test encoding and decoding an empty symbol table.
  Symtab symtab(objfile);
  symtab.PreloadSymbols();
  EncodeDecode(symtab);

  // Now encode and decode an actual symbol table from our yaml.
  Symtab *module_symtab = module_sp->GetSymtab();
  ASSERT_NE(module_symtab, nullptr);
  module_symtab->PreloadSymbols();
  EncodeDecode(*module_symtab);
}

TEST_F(SymtabTest, TestDecodeCStringMaps) {
  // Symbol tables save out the symbols, but they also save out the symbol table
  // name indexes. These name indexes are a map of sorted ConstString + T pairs
  // and when they are decoded from a file, they are no longer sorted since
  // ConstString objects can be sorted by "const char *" and the order in which
  // these strings are created won't be the same in a new process. We need to
  // ensure these name lookups happen correctly when we load the name indexes,
  // so this test loads a symbol table from a cache file from
  // "lldb/unittests/Symbol/Inputs/indexnames-symtab-cache" and make sure we
  // can correctly lookup each of the names in the symbol table.
  auto ExpectedFile = TestFile::fromYaml(R"(
--- !mach-o
FileHeader:
  magic:           0xFEEDFACF
  cputype:         0x100000C
  cpusubtype:      0x0
  filetype:        0x2
  ncmds:           16
  sizeofcmds:      744
  flags:           0x200085
  reserved:        0x0
LoadCommands:
  - cmd:             LC_SEGMENT_64
    cmdsize:         72
    segname:         __PAGEZERO
    vmaddr:          0
    vmsize:          4294967296
    fileoff:         0
    filesize:        0
    maxprot:         0
    initprot:        0
    nsects:          0
    flags:           0
  - cmd:             LC_SEGMENT_64
    cmdsize:         232
    segname:         __TEXT
    vmaddr:          4294967296
    vmsize:          16384
    fileoff:         0
    filesize:        16384
    maxprot:         5
    initprot:        5
    nsects:          2
    flags:           0
    Sections:
      - sectname:        __text
        segname:         __TEXT
        addr:            0x100003F64
        size:            76
        offset:          0x3F64
        align:           2
        reloff:          0x0
        nreloc:          0
        flags:           0x80000400
        reserved1:       0x0
        reserved2:       0x0
        reserved3:       0x0
        content:         80018052C0035FD6E0028052C0035FD640048052C0035FD6FF8300D1FD7B01A9FD43009108008052E80B00B9BFC31FB8F4FFFF97F5FFFF97F6FFFF97E00B40B9FD7B41A9FF830091C0035FD6
      - sectname:        __unwind_info
        segname:         __TEXT
        addr:            0x100003FB0
        size:            80
        offset:          0x3FB0
        align:           2
        reloff:          0x0
        nreloc:          0
        flags:           0x0
        reserved1:       0x0
        reserved2:       0x0
        reserved3:       0x0
        content:         010000001C000000000000001C000000000000001C00000002000000643F00003400000034000000B13F00000000000034000000030000000C0002001400020000000001180000000000000400000002
  - cmd:             LC_SEGMENT_64
    cmdsize:         72
    segname:         __LINKEDIT
    vmaddr:          4294983680
    vmsize:          16384
    fileoff:         16384
    filesize:        994
    maxprot:         1
    initprot:        1
    nsects:          0
    flags:           0
  - cmd:             LC_DYLD_CHAINED_FIXUPS
    cmdsize:         16
    dataoff:         16384
    datasize:        56
  - cmd:             LC_DYLD_EXPORTS_TRIE
    cmdsize:         16
    dataoff:         16440
    datasize:        80
  - cmd:             LC_SYMTAB
    cmdsize:         24
    symoff:          16528
    nsyms:           25
    stroff:          16928
    strsize:         176
  - cmd:             LC_DYSYMTAB
    cmdsize:         80
    ilocalsym:       0
    nlocalsym:       20
    iextdefsym:      20
    nextdefsym:      5
    iundefsym:       25
    nundefsym:       0
    tocoff:          0
    ntoc:            0
    modtaboff:       0
    nmodtab:         0
    extrefsymoff:    0
    nextrefsyms:     0
    indirectsymoff:  0
    nindirectsyms:   0
    extreloff:       0
    nextrel:         0
    locreloff:       0
    nlocrel:         0
  - cmd:             LC_LOAD_DYLINKER
    cmdsize:         32
    name:            12
    Content:         '/usr/lib/dyld'
    ZeroPadBytes:    7
  - cmd:             LC_UUID
    cmdsize:         24
    uuid:            3E94866E-0D1A-39BD-975B-64E8F1FDBAAE
  - cmd:             LC_BUILD_VERSION
    cmdsize:         32
    platform:        1
    minos:           786432
    sdk:             787200
    ntools:          1
    Tools:
      - tool:            3
        version:         49938432
  - cmd:             LC_SOURCE_VERSION
    cmdsize:         16
    version:         0
  - cmd:             LC_MAIN
    cmdsize:         24
    entryoff:        16252
    stacksize:       0
  - cmd:             LC_LOAD_DYLIB
    cmdsize:         56
    dylib:
      name:            24
      timestamp:       2
      current_version: 85943299
      compatibility_version: 65536
    Content:         '/usr/lib/libSystem.B.dylib'
    ZeroPadBytes:    6
  - cmd:             LC_FUNCTION_STARTS
    cmdsize:         16
    dataoff:         16520
    datasize:        8
  - cmd:             LC_DATA_IN_CODE
    cmdsize:         16
    dataoff:         16528
    datasize:        0
  - cmd:             LC_CODE_SIGNATURE
    cmdsize:         16
    dataoff:         17104
    datasize:        274
LinkEditData:
  NameList:
    - n_strx:          43
      n_type:          0x64
      n_sect:          0
      n_desc:          0
      n_value:         0
    - n_strx:          91
      n_type:          0x64
      n_sect:          0
      n_desc:          0
      n_value:         0
    - n_strx:          98
      n_type:          0x66
      n_sect:          0
      n_desc:          1
      n_value:         1651098491
    - n_strx:          1
      n_type:          0x2E
      n_sect:          1
      n_desc:          0
      n_value:         4294983524
    - n_strx:          152
      n_type:          0x24
      n_sect:          1
      n_desc:          0
      n_value:         4294983524
    - n_strx:          1
      n_type:          0x24
      n_sect:          0
      n_desc:          0
      n_value:         8
    - n_strx:          1
      n_type:          0x4E
      n_sect:          1
      n_desc:          0
      n_value:         8
    - n_strx:          1
      n_type:          0x2E
      n_sect:          1
      n_desc:          0
      n_value:         4294983532
    - n_strx:          157
      n_type:          0x24
      n_sect:          1
      n_desc:          0
      n_value:         4294983532
    - n_strx:          1
      n_type:          0x24
      n_sect:          0
      n_desc:          0
      n_value:         8
    - n_strx:          1
      n_type:          0x4E
      n_sect:          1
      n_desc:          0
      n_value:         8
    - n_strx:          1
      n_type:          0x2E
      n_sect:          1
      n_desc:          0
      n_value:         4294983540
    - n_strx:          162
      n_type:          0x24
      n_sect:          1
      n_desc:          0
      n_value:         4294983540
    - n_strx:          1
      n_type:          0x24
      n_sect:          0
      n_desc:          0
      n_value:         8
    - n_strx:          1
      n_type:          0x4E
      n_sect:          1
      n_desc:          0
      n_value:         8
    - n_strx:          1
      n_type:          0x2E
      n_sect:          1
      n_desc:          0
      n_value:         4294983548
    - n_strx:          167
      n_type:          0x24
      n_sect:          1
      n_desc:          0
      n_value:         4294983548
    - n_strx:          1
      n_type:          0x24
      n_sect:          0
      n_desc:          0
      n_value:         52
    - n_strx:          1
      n_type:          0x4E
      n_sect:          1
      n_desc:          0
      n_value:         52
    - n_strx:          1
      n_type:          0x64
      n_sect:          1
      n_desc:          0
      n_value:         0
    - n_strx:          2
      n_type:          0xF
      n_sect:          1
      n_desc:          16
      n_value:         4294967296
    - n_strx:          22
      n_type:          0xF
      n_sect:          1
      n_desc:          0
      n_value:         4294983532
    - n_strx:          27
      n_type:          0xF
      n_sect:          1
      n_desc:          0
      n_value:         4294983540
    - n_strx:          32
      n_type:          0xF
      n_sect:          1
      n_desc:          0
      n_value:         4294983524
    - n_strx:          37
      n_type:          0xF
      n_sect:          1
      n_desc:          0
      n_value:         4294983548
  StringTable:
    - ' '
    - __mh_execute_header
    - _bar
    - _baz
    - _foo
    - _main
    - '/Users/gclayton/Documents/objfiles/index-names/'
    - main.c
    - '/Users/gclayton/Documents/objfiles/index-names/main.o'
    - _foo
    - _bar
    - _baz
    - _main
    - ''
    - ''
    - ''
  FunctionStarts:  [ 0x3F64, 0x3F6C, 0x3F74, 0x3F7C ]
...
)");
  // This data was taken from a hex dump of the object file from the above yaml
  // and hexdumped so we can load the cache data in this test.
  const uint8_t symtab_cache_bytes[] = {
      0x01, 0x10, 0x3e, 0x94, 0x86, 0x6e, 0x0d, 0x1a, 0x39, 0xbd, 0x97, 0x5b,
      0x64, 0xe8, 0xf1, 0xfd, 0xba, 0xae, 0xff, 0x53, 0x54, 0x41, 0x42, 0x91,
      0x00, 0x00, 0x00, 0x00, 0x2f, 0x55, 0x73, 0x65, 0x72, 0x73, 0x2f, 0x67,
      0x63, 0x6c, 0x61, 0x79, 0x74, 0x6f, 0x6e, 0x2f, 0x44, 0x6f, 0x63, 0x75,
      0x6d, 0x65, 0x6e, 0x74, 0x73, 0x2f, 0x6f, 0x62, 0x6a, 0x66, 0x69, 0x6c,
      0x65, 0x73, 0x2f, 0x69, 0x6e, 0x64, 0x65, 0x78, 0x2d, 0x6e, 0x61, 0x6d,
      0x65, 0x73, 0x2f, 0x6d, 0x61, 0x69, 0x6e, 0x2e, 0x63, 0x00, 0x2f, 0x55,
      0x73, 0x65, 0x72, 0x73, 0x2f, 0x67, 0x63, 0x6c, 0x61, 0x79, 0x74, 0x6f,
      0x6e, 0x2f, 0x44, 0x6f, 0x63, 0x75, 0x6d, 0x65, 0x6e, 0x74, 0x73, 0x2f,
      0x6f, 0x62, 0x6a, 0x66, 0x69, 0x6c, 0x65, 0x73, 0x2f, 0x69, 0x6e, 0x64,
      0x65, 0x78, 0x2d, 0x6e, 0x61, 0x6d, 0x65, 0x73, 0x2f, 0x6d, 0x61, 0x69,
      0x6e, 0x2e, 0x6f, 0x00, 0x66, 0x6f, 0x6f, 0x00, 0x62, 0x61, 0x72, 0x00,
      0x62, 0x61, 0x7a, 0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x5f, 0x6d, 0x68,
      0x5f, 0x65, 0x78, 0x65, 0x63, 0x75, 0x74, 0x65, 0x5f, 0x68, 0x65, 0x61,
      0x64, 0x65, 0x72, 0x00, 0x53, 0x59, 0x4d, 0x42,
      // clang-format off
      0x03, 0x00, 0x00, 0x00, // DataFileCache's CURRENT_CACHE_VERSION
      // clang-format on
      0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x2a,
      0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x64, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x20, 0x01, 0x37,
      0x00, 0x00, 0x00, 0x00, 0x7b, 0xc3, 0x69, 0x62, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x66, 0x00,
      0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x32, 0x01, 0x6d, 0x00, 0x00,
      0x00, 0x01, 0x64, 0x3f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x08, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x02, 0x32, 0x01, 0x71, 0x00, 0x00, 0x00, 0x01,
      0x6c, 0x3f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x0c, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x02, 0x32, 0x01, 0x75, 0x00, 0x00, 0x00, 0x01, 0x74, 0x3f,
      0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x32, 0x01, 0x79, 0x00, 0x00, 0x00, 0x01, 0x7c, 0x3f, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x0f, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x12,
      0x02, 0x7e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
      0x00, 0x00, 0x64, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
      0x0f, 0x00, 0x01, 0x00, 0x43, 0x4d, 0x41, 0x50, 0x07, 0x00, 0x00, 0x00,
      0x6d, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x75, 0x00, 0x00, 0x00,
      0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x79, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x71, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
      0x7e, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00};

  ASSERT_THAT_EXPECTED(ExpectedFile, llvm::Succeeded());
  auto module_sp = std::make_shared<Module>(ExpectedFile->moduleSpec());
  ObjectFile *objfile = module_sp->GetObjectFile();
  ASSERT_NE(objfile, nullptr);

  // Test encoding and decoding an empty symbol table.
  DataExtractor data(symtab_cache_bytes, sizeof(symtab_cache_bytes),
                     eByteOrderLittle, 8);
  Symtab symtab(objfile);
  offset_t data_offset = 0;
  bool uuid_mismatch = false; // Gets set to true if signature doesn't match.
  const bool success = symtab.Decode(data, &data_offset, uuid_mismatch);
  ASSERT_EQ(success, true);
  ASSERT_EQ(uuid_mismatch, false);

  // Now make sure that name lookup works for all symbols. This indicates that
  // the Symtab::NameToIndexMap was decoded correctly and works as expected.
  const Symbol *symbol = nullptr;
  symbol = symtab.FindFirstSymbolWithNameAndType(ConstString("main"),
                                                 eSymbolTypeCode,
                                                 Symtab::eDebugAny,
                                                 Symtab::eVisibilityAny);
  ASSERT_NE(symbol, nullptr);
  symbol = symtab.FindFirstSymbolWithNameAndType(ConstString("foo"),
                                                 eSymbolTypeCode,
                                                 Symtab::eDebugAny,
                                                 Symtab::eVisibilityAny);
  ASSERT_NE(symbol, nullptr);
  symbol = symtab.FindFirstSymbolWithNameAndType(ConstString("bar"),
                                                 eSymbolTypeCode,
                                                 Symtab::eDebugAny,
                                                 Symtab::eVisibilityAny);
  ASSERT_NE(symbol, nullptr);
  symbol = symtab.FindFirstSymbolWithNameAndType(ConstString("baz"),
                                                 eSymbolTypeCode,
                                                 Symtab::eDebugAny,
                                                 Symtab::eVisibilityAny);
  ASSERT_NE(symbol, nullptr);
}

TEST_F(SymtabTest, TestSymbolFileCreatedOnDemand) {
  auto ExpectedFile = TestFile::fromYaml(R"(
--- !ELF
FileHeader:
  Class:           ELFCLASS64
  Data:            ELFDATA2LSB
  Type:            ET_EXEC
  Machine:         EM_X86_64
  Entry:           0x0000000000400180
Sections:
  - Name:            .text
    Type:            SHT_PROGBITS
    Flags:           [ SHF_ALLOC, SHF_EXECINSTR ]
    Address:         0x0000000000400180
    AddressAlign:    0x0000000000000010
    Content:         554889E58B042500106000890425041060005DC3
Symbols:
  - Name:            _start
    Type:            STT_FUNC
    Section:         .text
    Value:           0x0000000000400180
    Size:            0x0000000000000014
    Binding:         STB_GLOBAL
...
)");
  ASSERT_THAT_EXPECTED(ExpectedFile, llvm::Succeeded());
  auto module_sp = std::make_shared<Module>(ExpectedFile->moduleSpec());

  // The symbol file should not be created by default.
  Symtab *module_symtab = module_sp->GetSymtab(/*can_create=*/false);
  ASSERT_EQ(module_symtab, nullptr);

  // But it should be created on demand.
  module_symtab = module_sp->GetSymtab(/*can_create=*/true);
  ASSERT_NE(module_symtab, nullptr);

  // And we should be able to get it again once it has been created.
  Symtab *cached_module_symtab = module_sp->GetSymtab(/*can_create=*/false);
  ASSERT_EQ(module_symtab, cached_module_symtab);
}

TEST_F(SymtabTest, TestSymbolTableCreatedOnDemand) {
  const char *yamldata = R"(
--- !ELF
FileHeader:
  Class:   ELFCLASS64
  Data:    ELFDATA2LSB
  Type:    ET_EXEC
  Machine: EM_386
DWARF:
  debug_abbrev:
    - Table:
        - Code:            0x00000001
          Tag:             DW_TAG_compile_unit
          Children:        DW_CHILDREN_no
          Attributes:
            - Attribute:       DW_AT_addr_base
              Form:            DW_FORM_sec_offset
  debug_info:
    - Version:         5
      AddrSize:        4
      UnitType:        DW_UT_compile
      Entries:
        - AbbrCode:        0x00000001
          Values:
            - Value:           0x8 # Offset of the first Address past the header
        - AbbrCode:        0x0
  debug_addr:
    - Version: 5
      AddressSize: 4
      Entries:
        - Address: 0x1234
        - Address: 0x5678
  debug_line:
    - Length:          42
      Version:         2
      PrologueLength:  36
      MinInstLength:   1
      DefaultIsStmt:   1
      LineBase:        251
      LineRange:       14
      OpcodeBase:      13
      StandardOpcodeLengths: [ 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1 ]
      IncludeDirs:
        - '/tmp'
      Files:
        - Name:            main.cpp
          DirIdx:          1
          ModTime:         0
          Length:          0
)";
  llvm::Expected<TestFile> file = TestFile::fromYaml(yamldata);
  EXPECT_THAT_EXPECTED(file, llvm::Succeeded());
  auto module_sp = std::make_shared<Module>(file->moduleSpec());

  SymbolFile *symbol_file = module_sp->GetSymbolFile();
  // At this point, the symbol table is not created. This is because the above
  // yaml data contains the necessary sections in order for
  // SymbolFileDWARF::CalculateAbilities() to identify all abilities, saving it
  // from calling SymbolFileDWARFDebugMap::CalculateAbilities(), which
  // eventually loads the symbol table, which we don't want.

  // The symbol table should not be created if asked not to.
  Symtab *symtab = symbol_file->GetSymtab(/*can_create=*/false);
  ASSERT_EQ(symtab, nullptr);

  // But it should be created on demand.
  symtab = symbol_file->GetSymtab(/*can_create=*/true);
  ASSERT_NE(symtab, nullptr);

  // And we should be able to get it again once it has been created.
  Symtab *cached_symtab = symbol_file->GetSymtab(/*can_create=*/false);
  ASSERT_EQ(symtab, cached_symtab);
}
