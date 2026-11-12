//===- TracingCASBackedFileSystem.cpp ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/TracingCASBackedFileSystem.h"
#include "llvm/CAS/ObjectStore.h"

using namespace llvm;
using namespace llvm::cas;

const char TracingCASBackedFileSystem::ID = 0;

TracingCASBackedFileSystem::TracingCASBackedFileSystem(
    IntrusiveRefCntPtr<CASBackedFileSystem> FSArg)
    : FS(std::move(FSArg)),
      Tracing(makeIntrusiveRefCnt<vfs::TracingFileSystem>(FS)) {}

ErrorOr<vfs::Status> TracingCASBackedFileSystem::status(const Twine &Path) {
  return Tracing->status(Path);
}

bool TracingCASBackedFileSystem::exists(const Twine &Path) {
  return Tracing->exists(Path);
}

ErrorOr<std::unique_ptr<vfs::File>>
TracingCASBackedFileSystem::openFileForRead(const Twine &Path) {
  return Tracing->openFileForRead(Path);
}

vfs::directory_iterator
TracingCASBackedFileSystem::dir_begin(const Twine &Dir, std::error_code &EC) {
  return Tracing->dir_begin(Dir, EC);
}

ErrorOr<std::string>
TracingCASBackedFileSystem::getCurrentWorkingDirectory() const {
  return Tracing->getCurrentWorkingDirectory();
}

std::error_code
TracingCASBackedFileSystem::setCurrentWorkingDirectory(const Twine &Path) {
  return Tracing->setCurrentWorkingDirectory(Path);
}

std::error_code
TracingCASBackedFileSystem::getRealPath(const Twine &Path,
                                        SmallVectorImpl<char> &Output) {
  return Tracing->getRealPath(Path, Output);
}

void TracingCASBackedFileSystem::getDirectoryContentRealSources(
    const Twine &Dir, SmallVectorImpl<std::string> &Out) {
  Tracing->getDirectoryContentRealSources(Dir, Out);
}

std::error_code TracingCASBackedFileSystem::isLocal(const Twine &Path,
                                                    bool &Result) {
  return Tracing->isLocal(Path, Result);
}

Expected<const vfs::CachedDirectoryEntry *>
TracingCASBackedFileSystem::getDirectoryEntry(const Twine &Path,
                                              bool FollowSymlinks) const {
  // Not counted by vfs::TracingFileSystem, and not forwarded by it either.
  return FS->getDirectoryEntry(Path, FollowSymlinks);
}

bool TracingCASBackedFileSystem::isCASFS() const { return FS->isCASFS(); }

void TracingCASBackedFileSystem::visitChildFileSystems(
    VisitCallbackTy Callback) {
  Tracing->visit(Callback);
}

Expected<std::pair<std::unique_ptr<MemoryBuffer>, ObjectRef>>
TracingCASBackedFileSystem::getBufferAndObjectRefForFile(
    const Twine &Name, int64_t FileSize, bool RequiresNullTerminator,
    bool IsVolatile, bool IsText) {
  ++Tracing->NumOpenFileForReadCalls;
  return FS->getBufferAndObjectRefForFile(
      Name, FileSize, RequiresNullTerminator, IsVolatile, IsText);
}

ObjectStore &TracingCASBackedFileSystem::getCAS() const { return FS->getCAS(); }

IntrusiveRefCntPtr<CASBackedFileSystem>
TracingCASBackedFileSystem::createThreadSafeProxyFS() {
  return makeIntrusiveRefCnt<TracingCASBackedFileSystem>(
      FS->createThreadSafeProxyFS());
}

void TracingCASBackedFileSystem::printImpl(raw_ostream &OS, PrintType Type,
                                           unsigned IndentLevel) const {
  printIndent(OS, IndentLevel);
  OS << "TracingCASBackedFileSystem\n";
  if (Type == PrintType::Summary)
    return;
  Tracing->print(OS, Type, IndentLevel + 1);
}
