# Is the RTTI on `llvm::vfs::File` necessary? — design memo

Date: 2026-11-04
Scope: upstreaming clang's CAS compile-job caching from `swiftlang/llvm-project` to llvm.org/main.
Trees examined: `~/Code/llvm-cas-stage0` @ `cas/compile-job-caching` (`e3d9c9f587bc`, 17 commits);
`~/Code/swift-llvm` @ `next` (full downstream feature); `~/Code/org-llvm` @ `5165cd89b849` (upstream).
Experiments run in throwaway worktree `/tmp/cas-proto-wt` (branch `tmp/rtti-proto-throwaway`, deleted after).

## The question

`llvm/include/llvm/CAS/CASFileSystem.h:22` declares

```c++
class CASBackedFile : public RTTIExtends<CASBackedFile, vfs::File> {
  LLVM_ABI static const char ID;
  virtual cas::ObjectRef getObjectRefForContent() = 0;
};
```

making `llvm::vfs::File` an extensible-RTTI root so that a CAS object reference can be
recovered from an already-opened, generically-typed `vfs::File` via
`dyn_cast<llvm::cas::CASBackedFile>`. Is that necessary, merely convenient, or harmful?

## Bottom line

**Recommendation: Option D — delete `CASBackedFile` entirely.** Keep `CASBackedFileSystem`
(which uses the *already-upstream* `vfs::FileSystem` RTTI root) and make a by-path
`getObjectRefForFileContent(const Twine &)` its pure-virtual primitive instead of
`openCASBackedFileForRead`. Have the dependency scanner answer `path -> ObjectRef` from
the cache it *already maintains*, and delete all CAS concepts from `clang/lib/Basic/FileManager`.

The RTTI is **not necessary**, and it is **actively harmful**: it is the root cause of a
demonstrated crash, and the downstream fix for that crash is a hard LLVM layering inversion
(`LLVMSupport` -> `LLVMCAS`) that upstream will reject.

Headline evidence:

1. **Two of the four `dyn_cast<CASBackedFile>` sites are dead code.** Verified by compiling
   all 37 affected TUs with the parameter removed — zero errors.
2. **The status quo crashes** on `-ivfsoverlay` + `use-external-names: false`. Reproduced.
3. **The downstream fix for that crash makes `llvm/lib/Support/VirtualFileSystem.cpp`
   `#include "llvm/CAS/CASFileSystem.h"`** and define `cas::CASBackedFile::ID` inside
   LLVMSupport, with a self-acknowledged `FIXME: ... layer issue`.

---

## 1. Map of every consumer of the `vfs::File` RTTI

There are exactly **four** `dyn_cast<llvm::cas::CASBackedFile>` sites in `cas/compile-job-caching`
(verified by exhaustive grep over `clang/` and `llvm/`, excluding unit tests):

| # | Site | Status | Non-RTTI path available? |
|---|------|--------|--------------------------|
| 1 | `clang/lib/Basic/FileManager.cpp:564` (in `getBufferForFile`, under `if (CASContents)`) | **DEAD** — no caller ever passes `CASContents` | N/A, delete |
| 2 | `clang/lib/Basic/FileManager.cpp:597` (in `getBufferForFileImpl`, under `if (CASContents)`) | **DEAD** — same | N/A, delete |
| 3 | `clang/lib/Basic/FileManager.cpp:621` (`FileManager::getObjectRefForFileContent`) | **LIVE**, 2 callers, both `IncludeTreeBuilder` | **Yes** — ask the scanning FS by path |
| 4 | `clang/lib/DependencyScanning/DependencyScanningFilesystem.cpp:39` (`DependencyScanningWorkerFilesystem::readFile`) | **LIVE**, scan-side only | **Yes** — `dyn_cast<CASBackedFileSystem>` on the base FS it owns, then call the statically-typed `getBufferAndObjectRefForFile` |

Plus the *producers*, which exist only to satisfy the type and need no RTTI:
`CASFileSystem::VFSFile` (`llvm/lib/CAS/CASFileSystem.cpp:127`),
`CachingOnDiskFileSystemImpl::VFSFile` (`llvm/lib/CAS/CachingOnDiskFileSystem.cpp:225`),
`CASProvidingFile` (`llvm/lib/CAS/CASProvidingFileSystem.cpp:20`),
`IncludeTreeFileSystem::IncludeTreeFile` (`clang/lib/CAS/IncludeTree.cpp:518`),
`DepScanCASFile` (`clang/lib/DependencyScanning/DependencyScanningFilesystem.cpp:447`),
and in `next` only, `CASFileWithFixedStatus` (`llvm/lib/Support/VirtualFileSystem.cpp:2591`).

### Claim verification: the RTTI exists *only* to undo `openFileForRead`'s type erasure

**Verified.** `CASBackedFileSystem::openCASBackedFileForRead` returns a statically typed
`Expected<std::unique_ptr<CASBackedFile>>` (`llvm/include/llvm/CAS/CASFileSystem.h:59-61`), and
`openFileForRead` (`:51-57`) exists purely to launder it down to `vfs::File`:

```c++
ErrorOr<std::unique_ptr<vfs::File>> openFileForRead(const Twine &Path) override {
  auto F = openCASBackedFileForRead(Path);
  ...
  return std::move(*F);
}
```

Correspondingly, `CASBackedFileSystem::getObjectRefForFileContent` and
`getBufferAndObjectRefForFile` (`llvm/lib/CAS/CASFileSystem.cpp:21-45`) call
`openCASBackedFileForRead` directly and **use no RTTI at all**. Both are reachable today
without any change to `vfs::File`.

Verdict on necessity: **not necessary.** Every live consumer has a non-RTTI path.

---

## 2. The `FileManager` CAS out-parameter is dead code (verified by experiment)

`clang/include/clang/Basic/FileManager.h:277`, `:283`, `:301` carry
`std::optional<cas::ObjectRef> *CASContents = nullptr` on
`getBufferForFile` (x2) and `getBufferForFileImpl`.

**Finding: no caller anywhere passes it, in either tree.**

- Exhaustive call-site enumeration (joining multi-line calls and matching >= 5 positional
  arguments) found the maximum argument count at any `FileManager::getBufferForFile` call
  site is **5** (through `IsText`). The 6th parameter is never supplied. No
  member-pointer or macro indirection (`&FileManager::getBufferForFile` has no hits).
- In `~/Code/swift-llvm` `next` — the *full* downstream feature, including all Stage 3
  modules work — the only `CASContents` users outside `FileManager` itself are in
  `clang/unittests/Basic/FileManagerTest.cpp:545-598`. No `ASTReader`, `ASTWriter`,
  `ModuleManager`, or `ModuleDepCollector` caller exists.
- `CASBackedFileSystem::getBufferAndObjectRefForFile` (the "buffer + ref in one call" API
  that Stage 3 might plausibly want) likewise has **no non-test caller** in either tree
  (`llvm/unittests/CAS/CASProvidingFileSystemTest.cpp:54` only).

### Experimental confirmation

In the throwaway worktree I deleted the parameter from all three declarations and both
definitions, and reverted `getBufferForFileImpl` from its hand-rolled
open-then-`getBuffer` sequence back to the plain upstream one-liner:

```c++
return FS->getBufferForFile(Filename, FileSize, RequiresNullTerminator, isVolatile, IsText);
```

I then ran `-fsyntax-only` over **all 37 TUs in `clang/` that mention `getBufferForFile`**,
using the real build's `compile_commands.json` with the modified headers prepended to the
include path. This set includes `ASTReader.cpp`, `ASTWriter.cpp`, `ModuleManager.cpp`,
`GlobalModuleIndex.cpp`, `HeaderMap.cpp`, `PrecompiledPreamble.cpp`, `ASTUnit.cpp`,
`SourceManager.cpp` and `CachedDiagnostics.cpp`.

**Result: 37/37 compiled. Zero errors, zero failures.**

So consumers #1 and #2 in the table above are unreachable-with-effect, and the
hand-rolled `getBufferForFileImpl` rewrite (which exists *only* to expose the `vfs::File`
for casting, per its own comment at `FileManager.cpp:585-590`) is pure upstream diff with
no functional purpose. Deleting them reduces the core-clang diff to **one** method,
`getObjectRefForFileContent`, with exactly two call sites:
`clang/lib/DependencyScanning/IncludeTreeActionController.cpp:363` (the `ImplicitPCHInclude`
path) and `:466` (`IncludeTreeBuilder::addToFileList`).

This corroborates and sharpens the owner's hint: the apparent "core clang `FileManager` is
coupled to CAS" problem is driven by **one consumer**, and most of the surface is not driven
by any consumer at all.

---

## 3. Failure behaviour on cast failure: hard error, not silent corruption

The brief asked precisely what `IncludeTreeBuilder` does with `std::nullopt` /
`not_supported`. Traced:

- `FileManager::getObjectRefForFileContent` returns
  `std::make_error_code(std::errc::not_supported)` on cast failure
  (`clang/lib/Basic/FileManager.cpp:621-623`).
- `IncludeTreeBuilder::addToFileList` propagates it:
  `return llvm::createFileError(Filename, CASContents.getError());`
  (`IncludeTreeActionController.cpp:467-468`). Same at `:364-366` for the PCH path.
- `IncludeTreeActionController::finalize` reports it as
  `diag::err_cas_depscan_failed` and returns false (`:248-255`).

**So the include-tree path does NOT silently produce an incomplete tree.** There is no
fallback that re-reads and re-stores the bytes; it simply fails. Good news for
correctness, bad news for diagnosability — and it is worse than "fails" in practice:

### Reproduced crash (status quo, stage 0)

```
$ clang-scan-deps -compilation-database cdb.json \
    -format experimental-include-tree -cas-path cas
Failure value returned from cantFail wrapped call
failed to scan dependencies
UNREACHABLE executed at llvm/include/llvm/Support/Error.h:779!
```

Repro (full script in the appendix): a TU including a header reached through
`-ivfsoverlay` whose YAML sets **`"use-external-names": false`**. A plain
`clang -cc1 -emit-llvm` of the same TU succeeds; only include-tree generation aborts.

Mechanism: with `use-external-names: false` the file is known to `FileManager` by its
*virtual* path, so `openFileForRead` goes through
`RedirectingFileSystem::openFileForRead`, which wraps the inner file in
`FileWithFixedStatus` (`llvm/lib/Support/VirtualFileSystem.cpp`, upstream behaviour).
`dyn_cast<CASBackedFile>` then fails, `getObjectRefForFileContent` returns
`not_supported`, and `handleIncludeTreeToolResult` in `clang-scan-deps` funnels it into
`cantFail`. With `use-external-names: true` (the default) the external path is used, no
redirect wrapper is inserted, and the same test passes.

**This is exactly the silent-fragility failure mode the owner suspected, and it is a live
defect in the stage-0 series today** — two of my three experiment variants hit it.

### The downstream "fix" is a layering inversion

`next` repairs this by teaching core LLVM about CAS:

- `llvm/lib/Support/VirtualFileSystem.cpp:24-25` adds
  `#include "llvm/CAS/CASReference.h"` and `#include "llvm/CAS/CASFileSystem.h"`.
- `:2591` adds a second wrapper class `CASFileWithFixedStatus` duplicating
  `FileWithFixedStatus` plus a `getObjectRefForContent()` forwarder.
- `:2691-2693` adds a `dyn_cast<cas::CASBackedFile>` branch in
  `RedirectingFileSystem::openFileForRead`.
- `:3086-3088` defines `const char cas::CASBackedFile::ID = 0;` **inside LLVMSupport**,
  with the comment:

  > `// FIXME: Temporarily put the CASBackedFile::ID in Support library to workaround`
  > `// the layer issue that dyn_cast check for the type needs to happen here.`

`LLVMCAS` depends on `LLVMSupport`; this makes `LLVMSupport` reference a `LLVMCAS` symbol.
In stage 0 the ID is still correctly in `llvm/lib/CAS/CASFileSystem.cpp:17` — i.e.
**stage 0's RTTI looks clean only because it is also broken.** Continuing with option (C)
means either shipping the crash or shipping the inversion.

This is also the third data point in the "every intermediate wrapper must know about the
CAS type" pattern, alongside `DepScanCASFile` (see next section).

### Confirming the `DepScanCASFile` data point

The brief's reading is **confirmed**.
`clang/lib/DependencyScanning/DependencyScanningFilesystem.cpp:447` defines

```c++
class DepScanCASFile final : public llvm::cas::CASBackedFile { ... };
```

and its own doc comment states the purpose explicitly (`:441-446`): "Same as `DepScanFile`,
but additionally exposes the `cas::ObjectRef` ... so callers producing a CAS include-tree
(see IncludeTreeActionController.cpp) can record file contents without a separate CAS
store". `DepScanFile::create` (`:478-495`) branches on whether the cached entry has a ref
and returns one of two near-identical classes.

So the scanning filesystem's wrapper was indeed forced to re-derive from `CASBackedFile`
purely to keep a `dyn_cast` working two layers up. The pattern now has three instances
(`DepScanCASFile`, `CASFileWithFixedStatus`, and the duplicated-class shape of both), and
one *missing* instance that causes the crash above. A layering scheme in which correctness
requires every present and future `vfs::File` wrapper in LLVM and clang to know about CAS
is not a scheme that can be maintained — any upstream contributor adding a new
`vfs::FileSystem` decorator silently breaks CAS caching.

---

## 4. Alternative (A): scanner holds a direct handle to the `CASBackedFileSystem`

### Is the premise true? Partly — the handle is *threadable* but does not exist today.

Where the CAS-backed FS is actually injected:

- `clang/tools/clang-scan-deps/ClangScanDeps.cpp:1274-1279` —
  `Opts.MakeVFS = [CAS] { ... createCASProvidingFileSystem(CAS, physicalFS) ... }`
- `clang/tools/driver/cc1depscan_main.cpp:184-189` — same shape.
- `clang/lib/Tooling/DependencyScanningTool.cpp` and `clang/unittests/CAS/IncludeTreeTest.cpp:54`.

So it is injected at the **bottom** of the stack, by the *tool*, as a factory lambda. The
`DependencyScanningWorker` constructor then does
`auto BaseFS = Service.getOpts().MakeVFS();` and wraps it
(`DependencyScanningWorker.cpp:1058-1068`): optional `TracingFileSystem`, then
`DependencyScanningWorkerFilesystem DepFS`. `makeEffectiveVFS` (`:1072-1086`) optionally adds
a client `OverlayFileSystem` on top, and clang later adds `RedirectingFileSystem` for
`-ivfsoverlay` via `createVFSFromCompilerInvocation`.

Meanwhile the consumer, `IncludeTreeActionController`, is constructed from
`IncludeTreeCompilation{CASOpts, CAS, Cache}` (`DependencyScanningTool.cpp:504-506`,
`:294-296`, `:308-310`) and receives only a `cas::ObjectStore &` — **no filesystem handle**
(`IncludeTreeActionController.cpp:28-31`, `IncludeTreeBuilder` ctor `:67-68`).

**Verdict: the premise is false today but cheap to make true.** `DependencyScanningWorker`
holds the freshly-created `BaseFS` and could `dyn_cast<llvm::cas::CASBackedFileSystem>` it
— a `vfs::FileSystem`-level cast, which needs **no new RTTI** (see section 6) — and store
an `IntrusiveRefCntPtr<CASBackedFileSystem>`.
`DependencyScanningTool::createActionController(DependencyScanningWorker &Worker, ...)`
already takes the `Worker` by reference, so the handle can reach the controller with one
accessor and one extra constructor argument.

### But (A) as literally stated has a real performance regression

`CASProvidingFileSystem` has **no cache**: `openCASBackedFileForRead`
(`llvm/lib/CAS/CASProvidingFileSystem.cpp:86-101`) re-opens the underlying file, reads the
whole buffer, and calls `DB->storeFromString` — i.e. **re-hashes the contents** on every
call. Today that cost is paid once per file because `DependencyScanningWorkerFilesystem`
caches the resulting `ObjectRef` on its entry
(`DependencyScanningFilesystem.h:45`, `:99-104`, `:314-316`) and the `FileManager`'s
`openFileForRead` is served from that cache. If `IncludeTreeBuilder` instead asks the
*bottom* `CASProvidingFileSystem` by path, every include-tree file is read and hashed a
second time. That is a straight regression for the hot path of a dependency scanner.

### Does (A) eliminate the `FileManager` out-param?

**Yes — but so does doing nothing**, because the out-param is already dead (section 2).
What (A) genuinely eliminates is `FileManager::getObjectRefForFileContent`, the only *live*
core-clang CAS surface, because `IncludeTreeBuilder` would call the CAS FS directly and
never route through `FileManager`. After that, `clang/lib/Basic/FileManager.{h,cpp}`
contains **zero** CAS references. That is a real and significant win, and it is the single
most valuable part of the proposal.

### Hard cases for (A)

| Case | Behaviour under (A) | Behaviour today (C) |
|---|---|---|
| Plain on-disk header | Works | Works |
| `-ivfsoverlay`, `use-external-names: true` | Works (external path queried) | Works |
| `-ivfsoverlay`, `use-external-names: false` | Fails cleanly: bottom FS has no such path | **Crashes** (verified) |
| Client `OverlayFS` / in-memory inputs (IDE, `cc1depscan`) | Fails cleanly | Fails (`not_supported` -> `cantFail`) |
| `SourceManager::overrideFileContents` buffers | Returns the **stale on-disk** ref | Same bug today — `getObjectRefForFileContent` also re-opens via the VFS and bypasses the override |
| Predefines / non-file buffers | Unaffected — `getObjectForBuffer` uses `DB.storeFromString` (`IncludeTreeActionController.cpp:437-449`) | Same |
| PCH (`ImplicitPCHInclude`) | Works if the path is on the base FS | Works |
| Header maps, module `.pcm` | Not in the include tree at all — see section 5 | Same |

Note the overall shape: **(A) is behaviourally equivalent to (C) on every case**, because
today's `getObjectRefForFileContent` *also* just does `FS->openFileForRead(Name)` and casts
(`FileManager.cpp:615-626`). The only difference is *which* filesystem is asked. (A) is
therefore not a correctness regression, and it converts one crash into a clean error.

---

## 5. Alternative (B): drop the CAS-backed FS from the scan, build the tree from scratch

### The owner's fear is founded — and it is already a live defect, not a hypothetical

The owner wrote: *"I'm afraid some part of the compiler opens a random file that isn't part
of the include-tree but must be present in the CAS-backed VFS we give to the actual
compilation."* I set out to determine whether the set of files the replay compilation must
be able to open equals the set `IncludeTreeBuilder` observes.

**It does not. It is a strict superset, and the gap is demonstrable today.**

On replay, `createBaseFS` (`clang/lib/Frontend/CompilerInvocation.cpp:5709-5780`) *replaces*
the real filesystem with `IncludeTreeFileSystem` (`clang/lib/CAS/IncludeTree.cpp:514`) when
`-fcas-include-tree` is given, and `createVFSFromCompilerInvocation` (`:5788-5797`) then
layers `-ivfsoverlay` files **on top of it**:

```c++
return createVFSFromOverlayFiles(
    CI.getHeaderSearchOpts().VFSOverlayFiles, Diags,
    createBaseFS(CI.getFrontendOpts(), Diags, std::move(BaseFS), std::move(CAS)));
```

`configureInvocationForCaching` does not strip `HeaderSearchOpts::VFSOverlayFiles`, so the
replay command line still carries `-ivfsoverlay`. But the overlay YAML itself is **not** in
the include-tree file list. Verified end-to-end:

```
$ clang-scan-deps ... -format experimental-include-tree -cas-path cas
llvmcas://c3753c... - /tmp/casexp/t.c
Files:
/tmp/casexp/t.c                       llvmcas://f7ec02...
/tmp/casexp/real/actual_header.h      llvmcas://4731d5...      <- no overlay.yaml

$ clang -cc1 -triple arm64-apple-macosx -emit-llvm -x c \
    -ivfsoverlay /tmp/casexp/overlay.yaml -I /tmp/casexp/vdir \
    -fcas-path /tmp/casexp/cas -fcas-include-tree llvmcas://c3753c... -o tree.ll
fatal error: virtual filesystem overlay file '/tmp/casexp/overlay.yaml' not found
```

That is a **pre-existing correctness bug in the status quo**, independent of this memo's
question, and worth filing separately. `next` has the same gap: its `finishIncludeTree`
also never adds `VFSOverlayFiles` (checked `next:.../IncludeTreeActionController.cpp:680-705`).

### What this means for (B)

Crucially, it means **there is no automatic set-capture to lose.** I had expected the
CAS-backed scanning FS to be recording the set of accessed files; it is not:

- `CASProvidingFileSystem` is a pure pass-through decorator that ingests contents *on
  demand* and keeps no record (`llvm/lib/CAS/CASProvidingFileSystem.cpp:43-105`).
- `CachingOnDiskFileSystem` — which *does* accumulate a tree of everything accessed — is
  **not used anywhere in clang** in either tree (only
  `clang/unittests/DependencyScanning/DependencyScanningFilesystemTest.cpp`).

The include tree is already assembled from (i) preprocessor `FileChanged` callbacks and
(ii) a **hand-maintained explicit list** in `IncludeTreeBuilder::finishIncludeTree`
(`IncludeTreeActionController.cpp:304-345`): `LangOpts::NoSanitizeFiles`,
`CodeGenOpts::ProfileInstrumentUsePath`, `SampleProfileFile`, `ProfileRemappingFile`, and
`$SYSROOT/SDKSettings.json`. That list is the mechanism for "files the compiler opens that
the preprocessor never sees", and it is provably incomplete.

So the owner's fear is **founded as a statement about the file set**, but it is *not* a
reason to prefer (C) or (A) over (B) — all three share the identical exposure, because none
of them capture the set automatically.

### (B) should still be rejected, for different reasons

1. It would require `IncludeTreeBuilder` to read and hash every file's bytes itself, giving
   up the dedup that `CASProvidingFileSystem` + the `DepFS` ref cache provide. The scanner
   would do strictly more hashing.
2. `IncludeTreeFileSystem` is still needed on the *replay* side regardless (it is what
   serves the tree's contents to the compiler), and it is a `CASBackedFileSystem` today. (B)
   addresses only the scan side.
3. It does not actually answer the RTTI question: the remaining
   `DependencyScanningFilesystem.cpp:39` cast would still exist.

**(B) is a much larger change than (A) or (D) for strictly less benefit. Reject.**

---

## 6. Upstream-acceptability: is `vfs::File` RTTI actually novel?

The brief asked me to be accurate rather than assume reviewers would object. Checked
upstream `~/Code/org-llvm` @ `5165cd89b849`,
`llvm/include/llvm/Support/VirtualFileSystem.h`:

- `vfs::FileSystem` **is already an extensible-RTTI root**:
  `class FileSystem : public llvm::ThreadSafeRefCountedBase<FileSystem>, public RTTIExtends<FileSystem, RTTIRoot>` (`:268-270`),
  with `OverlayFileSystem` (`:406`), `ProxyFileSystem` (`:470`),
  `InMemoryFileSystem` (`:562`), `RedirectingFileSystem` (`:803`) and
  `TracingFileSystemImpl` (`:1188`) all using `RTTIExtends` + `static const char ID`.
- `vfs::File` (`:115`) is a **plain class with no RTTI**.

So making `vfs::File` an RTTI root is genuinely novel, **but it is an idiomatic, one-line
extension of a pattern already established in the same header**. On its own, I judge it
**unlikely to be a blocking objection** — "`FileSystem` does this, why not `File`" is a
defensible review answer, and `ExtensibleRTTI` is exactly the mechanism LLVM provides for
out-of-tree-ish type extension.

**The blocking objections are elsewhere:**

1. **`LLVMSupport` -> `LLVMCAS` layering inversion** (`next` only, but unavoidable if (C) is
   to be made correct). Including `llvm/CAS/CASFileSystem.h` from
   `llvm/lib/Support/VirtualFileSystem.cpp` and defining `cas::CASBackedFile::ID` there is a
   near-certain rejection, and the existing `FIXME` concedes it.
2. **CAS concepts in `clang/lib/Basic/FileManager`** — an `std::optional<cas::ObjectRef> *`
   out-parameter on a central API, with no callers, plus a rewrite of
   `getBufferForFileImpl` away from `FS->getBufferForFile()` for no observable reason. This
   is the kind of thing reviewers reliably catch and it is indefensible once the "no
   callers" fact is on the table.
3. **The maintenance contract**, which is the strongest principled objection: under the RTTI
   design, *any* `vfs::File` decorator anywhere in LLVM or clang silently disables CAS
   caching unless it is taught to forward the CAS type. I have a reproducible crash
   demonstrating this. A reviewer who understands that will ask for the by-path design.

Note that `dyn_cast<CASBackedFileSystem>` on a **filesystem** is fully consistent with
existing upstream practice and needs no new core-LLVM change at all. This asymmetry is the
heart of the recommendation: do the polymorphism at the `FileSystem` layer, where LLVM
already supports it, not at the `File` layer, where it requires new core infrastructure and
has no safe propagation story through decorators.

---

## 7. Option (D): delete `CASBackedFile`; make the ref-by-path query the primitive

This is the fourth option and my recommendation. It is a refinement of (A) that fixes (A)'s
performance regression.

### Shape

1. **Delete `class CASBackedFile`** and the `vfs::File` RTTI root. Delete
   `CASFileSystem.h`'s `openCASBackedFileForRead` / `openFileForRead` override pair.
2. **Make `CASBackedFileSystem` the sole CAS-aware interface**, with the by-path query as
   its pure-virtual primitive:

   ```c++
   class CASBackedFileSystem
       : public llvm::RTTIExtends<CASBackedFileSystem, vfs::FileSystem> {
   public:
     LLVM_ABI static const char ID;            // FileSystem RTTI: already upstream
     virtual Expected<cas::ObjectRef> getObjectRefForFileContent(const Twine &) = 0;
     virtual Expected<std::pair<std::unique_ptr<MemoryBuffer>, cas::ObjectRef>>
         getBufferAndObjectRefForFile(const Twine &, ...) = 0;
     virtual IntrusiveRefCntPtr<CASBackedFileSystem> createThreadSafeProxyFS() = 0;
   };
   ```

   All five implementations already hold the ref keyed by path and can serve this trivially:
   `CASFileSystem` (`CASFileSystem.cpp:146`, `*Entry->getRef()`),
   `CachingOnDiskFileSystemImpl` (`CachingOnDiskFileSystem.cpp:244`, same),
   `CASProvidingFileSystem` (open + `storeFromString`, as it does today),
   `IncludeTreeFileSystem` (its `Files` `StringMap` already stores `ContentsRef`,
   `IncludeTree.cpp:558-561`), and `DependencyScanningWorkerFilesystem` (its cached
   entries already carry `CASContents`).
   `DepScanCASFile`, `CASProvidingFile`, `IncludeTreeFile`, `CASFileSystem::VFSFile`,
   `CachingOnDiskFileSystemImpl::VFSFile` and (in `next`) `CASFileWithFixedStatus` all
   collapse back into their plain `vfs::File` counterparts — a net **deletion** of ~5
   duplicated wrapper classes.
3. **Scanner side:** `DependencyScanningWorkerFilesystem` keeps an
   `IntrusiveRefCntPtr<cas::CASBackedFileSystem>` obtained by
   `dyn_cast<CASBackedFileSystem>(BaseFS)` in its constructor, and `readFile` calls
   `CASFS->getBufferAndObjectRefForFile(...)` — statically typed, one open instead of two.
   This removes consumer #4.
4. **Consumer side:** make `DependencyScanningWorkerFilesystem` itself answer
   `path -> ObjectRef` from its existing cache. **The API already exists and is public:**

   ```c++
   llvm::ErrorOr<EntryRef> getOrCreateFileSystemEntry(StringRef Filename);  // :357
   std::optional<cas::ObjectRef> EntryRef::getObjectRefForContent() const;  // :314
   ```

   `IncludeTreeBuilder` calls that instead of `FM.getObjectRefForFileContent`. No re-read,
   no re-hash, no extra open — strictly cheaper than today. This removes consumer #3 and
   lets `FileManager::getObjectRefForFileContent` be deleted.
5. **Delete the dead out-param** and restore `getBufferForFileImpl` to the upstream
   one-liner (already prototyped and compile-verified, section 2).

### Result

- `clang/lib/Basic/FileManager.{h,cpp}`: **zero** CAS references. No `#include` of any CAS
  header, no `cas::` identifier.
- `llvm/lib/Support/VirtualFileSystem.cpp`: **zero** CAS references, now and in future.
  The layering inversion never needs to be introduced.
- `llvm/include/llvm/Support/VirtualFileSystem.h`: **unmodified**. `vfs::File` stays a plain
  class.
- Core-LLVM change required for CAS caching: **none** beyond the new `llvm/CAS/` files,
  which are new-library additions rather than modifications to central types.

### Is (D) "just RTTI with extra steps"?

No, and the distinction is load-bearing. The brief floated a non-RTTI virtual hook on
`vfs::File` returning an opaque content identity with a default null implementation. That
*would* be RTTI with extra steps — worse, in fact, because it puts a CAS-shaped concept
into `vfs::File`'s vtable for every user of LLVM's VFS, and it still has the decorator
propagation problem (`FileWithFixedStatus` would still need to forward it).

(D) is different in kind: the CAS-awareness lives on the **filesystem**, which is a
long-lived object with a known identity that the scanner itself created, rather than on a
**transient, freely-wrapped** per-open object. There is nothing to launder.

### Residual weakness of (D), stated honestly

(D) does not make the `-ivfsoverlay` + `use-external-names: false` case *work*; it makes it
fail cleanly instead of crashing. The root cause there is a **path-identity** problem, not an
RTTI problem: `FileEntryRef::getName()` yields the virtual path while the scanning FS caches
under the external path. Fixing it properly requires either recording the external path in
the tree or adding the overlay YAMLs to the file list (which is needed anyway, section 5).
That work is orthogonal and equally required under (A), (B) and (C) — under (C) it is merely
*masked* for the subset of cases that `CASFileWithFixedStatus` happens to cover, at the cost
of the layering inversion.

---

## 8. Options side by side

| | Core-LLVM change | Core-clang CAS surface | `-ivfsoverlay` (ext-names off) | Extra hashing | Upstream risk |
|---|---|---|---|---|---|
| **(A)** scanner holds CAS FS handle, asks bottom FS by path | none (`FileSystem` RTTI already upstream) | none | clean error | **yes, re-hashes every file** | low |
| **(B)** no CAS FS in scan, tree built from scratch | none | none | clean error | yes, builder hashes everything | low, but large diff for no gain |
| **(C)** status quo, keep `File` RTTI | `vfs::File` becomes RTTI root; **plus** Support->CAS inversion once the crash is fixed | dead out-param + `getObjectRefForFileContent` | **crashes** today; works in `next` via the inversion | no | **high** (layering inversion is the blocker, not the RTTI itself) |
| **(D)** delete `CASBackedFile`, ref-by-path on `CASBackedFileSystem`, DepFS answers from its cache | **none** | **none** | clean error | **no — one fewer open than today** | **lowest** |

## 9. Recommendation

**Adopt (D).** Concretely:

1. Delete the `std::optional<cas::ObjectRef> *CASContents` out-parameter from
   `FileManager::getBufferForFile` (x2) and `getBufferForFileImpl`, and restore
   `getBufferForFileImpl` to `return FS->getBufferForFile(...)`. Delete the corresponding
   unit-test block. **Already prototyped; 37/37 TUs compile.**
2. In `llvm/CAS/CASFileSystem.h`, delete `class CASBackedFile` and the
   `openCASBackedFileForRead` / `openFileForRead` pair; promote
   `getObjectRefForFileContent` and `getBufferAndObjectRefForFile` to pure virtuals on
   `CASBackedFileSystem`. Collapse the five `CASBackedFile` subclasses back to plain
   `vfs::File` implementations.
3. Give `DependencyScanningWorkerFilesystem` a
   `dyn_cast<cas::CASBackedFileSystem>(BaseFS)` handle and use
   `getBufferAndObjectRefForFile` in `readFile`.
4. Point `IncludeTreeBuilder` at `DepFS->getOrCreateFileSystemEntry(Path)` /
   `EntryRef::getObjectRefForContent()`; thread the `DepFS` (or a narrow
   `path -> optional<ObjectRef>` callback) through
   `DependencyScanningTool::createActionController` into `IncludeTreeActionController`.
   Delete `FileManager::getObjectRefForFileContent`.
5. Replace the `cantFail` in `clang-scan-deps`'s `handleIncludeTreeToolResult` with proper
   error reporting, and give the "no CAS reference for this path" case a real diagnostic
   instead of `std::errc::not_supported`.
6. **Separately** (pre-existing bug, not blocking): add
   `HeaderSearchOpts::VFSOverlayFiles` to `IncludeTreeBuilder::finishIncludeTree`'s explicit
   file list, and add a lit test for `-ivfsoverlay` + include-tree in both
   `use-external-names` modes. There is currently **no** such test
   (`clang/test/CAS/` contains six tests, none mentioning `ivfsoverlay`).

### Blast radius

Small, and concentrated in one commit of the series.

- `llvm/include/llvm/CAS/CASFileSystem.h`, `llvm/lib/CAS/CASFileSystem.cpp`,
  `llvm/lib/CAS/CASProvidingFileSystem.cpp`, `llvm/lib/CAS/CachingOnDiskFileSystem.cpp`,
  `llvm/include/llvm/CAS/CachingOnDiskFileSystem.h` — these belong to the **earlier** stage
  commit `fae5c2f68852 [CAS] Add CAS-backed VFS, output capture, and configuration helper`,
  which is **not yet in llvm.org/main** (`~/Code/org-llvm` has no
  `llvm/include/llvm/CAS/CASFileSystem.h`). The window to change the design is still open —
  this is an edit to a pending commit, not a revert of landed code.
- `clang/include/clang/Basic/FileManager.h`, `clang/lib/Basic/FileManager.cpp`,
  `clang/include/clang/DependencyScanning/DependencyScanningFilesystem.h`,
  `clang/lib/DependencyScanning/DependencyScanningFilesystem.cpp`,
  `clang/lib/DependencyScanning/IncludeTreeActionController.cpp`,
  `clang/lib/CAS/IncludeTree.cpp`, `clang/lib/Tooling/DependencyScanningTool.cpp` — all in or
  adjacent to the single commit `322a3156dc24 [clang][CAS] Add IncludeTreeActionController
  and scanning-side include-tree production`.
- Tests: `clang/unittests/Basic/FileManagerTest.cpp` (delete two CAS cases),
  `llvm/unittests/CAS/CASProvidingFileSystemTest.cpp` (retarget to the FS-level API),
  `clang/unittests/Tooling/DependencyScannerTest.cpp` (two `dyn_cast<CASBackedFile>` sites
  exist in `next` at `:265`, `:289`; retarget).
- `llvm/lib/Support/VirtualFileSystem.cpp` — **not touched at all**, versus `next` which
  touches it in four places.

Estimate: ~10-12 files, 2 commits amended, net line count negative.

---

## 10. What I could not verify

- **I did not build or run `next`.** The claim that `next` fixes the
  `use-external-names: false` crash is from reading
  `RedirectingFileSystem::openFileForRead` (`next:llvm/lib/Support/VirtualFileSystem.cpp:2691-2697`)
  and `CASFileWithFixedStatus` (`:2591-2613`). The code clearly handles that path, but I did
  not confirm the end-to-end scan succeeds there. **Not verified.**
- **I did not build option (D).** The only part I built and ran is the out-param removal
  (section 2), and that was `-fsyntax-only` over 37 TUs, not a full link + `check-clang`.
  A full build and test run is still needed before amending the series. The claims that each
  `CASBackedFileSystem` implementation can serve `getObjectRefForFileContent` by path are
  from reading the five implementations; I did not write the code.
- **Header maps and module `.pcm` files on replay.** I inferred from section 5's mechanism
  that a header map reached via `HeaderMap.cpp:54` would also be absent from the include
  tree and so unavailable on replay, but I **did not construct a repro**. Likewise for
  explicit module inputs. If the superset problem matters beyond overlay YAMLs, these are
  the next places to look. **Not verified.**
- **`SourceManager::overrideFileContents` / `-remap-file`.** I reasoned from
  `IncludeTreeBuilder::getObjectForFile` (`IncludeTreeActionController.cpp:405-428`) that a
  file-backed `FileID` with overridden contents takes the `addToFileList` path and would get
  the on-disk ref rather than the overridden bytes. I did not test it. If true it is a
  pre-existing correctness bug independent of this memo. **Not verified.**
- **Whether `IncludeTreeBuilder` can always reach the `DepFS` under step (D).4.** The
  `getOrCreateFileSystemEntry` API is public and the cache is populated, but
  `DependencyScanningWorker::DepFS` is a private member with no accessor
  (`DependencyScanningWorker.h:128`, `friend class CompilerInstanceWithContext`). Adding an
  accessor is trivial; I did not check whether the by-name / module-scanning entry points
  (`computeDependenciesByName`, the `BuilderStack` push for modules at
  `IncludeTreeActionController.cpp:229`) all have the DepFS live at the right time.
  **Not verified.**
- **Thread safety.** `createThreadSafeProxyFS` exists on `CASBackedFileSystem`
  (`CASFileSystem.h:63-64`) for per-thread working directories. I did not analyse whether
  routing ref queries through the shared `DepFS` instead of a per-thread proxy introduces
  contention. `DependencyScanningWorkerFilesystem` is per-worker, so probably fine, but
  **not verified.**
- **Performance numbers.** The claim that (A) re-hashes and (D) does not is from reading
  `CASProvidingFileSystem::openCASBackedFileForRead`
  (`llvm/lib/CAS/CASProvidingFileSystem.cpp:86-101`, no cache, unconditional
  `DB->storeFromString`). I did not measure.

---

## Appendix: reproducers

Both scripts assume `B=~/Code/llvm-cas-stage0/build/debug/bin`. The build is AArch64-only,
hence `arm64-apple-macosx`.

### A. Crash: `-ivfsoverlay` with `use-external-names: false`

```sh
T=/tmp/casexp2; rm -rf $T; mkdir -p $T/real $T/cas
echo 'struct S { int x; };'                                  > $T/real/actual_header.h
printf '#include "virtual_header.h"\nint test(struct S *s) { return s->x; }\n' > $T/t.c
cat > $T/overlay.yaml <<YAML
{ "version": 0, "use-external-names": false,
  "roots": [ { "name": "$T/vdir", "type": "directory",
    "contents": [ { "name": "virtual_header.h", "type": "file",
                    "external-contents": "$T/real/actual_header.h" } ] } ] }
YAML
cat > $T/cdb.json <<JSON
[ { "directory": "$T",
    "command": "clang -fsyntax-only $T/t.c -ivfsoverlay $T/overlay.yaml -I $T/vdir",
    "file": "$T/t.c" } ]
JSON
$B/clang -cc1 -triple arm64-apple-macosx -emit-llvm $T/t.c \
    -ivfsoverlay $T/overlay.yaml -I $T/vdir -o $T/direct.ll   # succeeds
$B/clang-scan-deps -compilation-database $T/cdb.json \
    -format experimental-include-tree -cas-path $T/cas        # UNREACHABLE, exit 134
```

Observed: `Failure value returned from cantFail wrapped call` /
`UNREACHABLE executed at llvm/include/llvm/Support/Error.h:779`.
Changing `"use-external-names"` to `true` makes it pass.

### B. Replay gap: overlay YAML missing from the include tree

Same setup but with `"use-external-names": true`. The scan succeeds and prints a file list
containing only `t.c` and `real/actual_header.h`. Replaying with the overlay still on the
command line fails:

```sh
$B/clang -cc1 -triple arm64-apple-macosx -emit-llvm -x c \
    -ivfsoverlay $T/overlay.yaml -I $T/vdir \
    -fcas-path $T/cas -fcas-include-tree <ID> -o $T/tree.ll
# fatal error: virtual filesystem overlay file '/tmp/casexp/overlay.yaml' not found
```

### C. Dead out-param (compile check)

In a throwaway worktree, remove `CASContents` from `FileManager.h:277,283,301` and both
definitions in `FileManager.cpp`, restore `getBufferForFileImpl` to
`FS->getBufferForFile(...)`, then `-fsyntax-only` every TU in `clang/` that mentions
`getBufferForFile`, using the real build's `compile_commands.json` with
`-I<worktree>/clang/include` prepended. 37 TUs, 0 failures.

---

# Implementation on `next`

Date: 2026-11-05
Tree: `~/Code/swift-llvm-rtti`, branch `cas/vfs-file-rtti-option-d`, forked from
`origin/next` @ `8af41bd656fb`.
Baseline worktree for comparison: `~/Code/swift-llvm-base` (detached @ `8af41bd656fb`).
Builds: `build/release` (assertions ON), config `AArch64;ARM;X86;RISCV`.

## Bottom line

**Option D works on `next`, but not in the shape section 7 of this memo proposed.**
The by-path step (7.4 / 9.4 — "have `DependencyScanningWorkerFilesystem` answer
`path -> ObjectRef` from its existing cache") is **unsound and would have been a
regression on `next`**. It was replaced by a **content-based** primitive: the
include-tree builder derives each reference from the *bytes the compilation
actually read*, and the scanner captures the reference at the single point where
the file is opened and the CAS filesystem is statically known.

With that substitution, all of Option D's structural goals are met:

- `vfs::File` is a plain class again; `CASBackedFile` is deleted.
- `llvm/lib/Support/VirtualFileSystem.cpp`: **zero** CAS references
  (`grep -c 'cas::\|CAS'` = 0). The `LLVMSupport -> LLVMCAS` layering inversion,
  the `CASFileWithFixedStatus` duplicate wrapper, and the
  `cas::CASBackedFile::ID` definition in LLVMSupport are all gone.
- `clang/{include/clang,lib}/Basic/FileManager.{h,cpp}`: **zero** CAS references.
- Behaviour is bit-for-bit preserved on the discriminating test pair: the
  include-tree root IDs for `-ivfsoverlay` with `use-external-names` true and
  false are **identical to unmodified `next`** (`19937f36...` and `279edec...`).

## Corrections to this memo, established by measurement on `next`

### C1. `next` does **not** crash on `-ivfsoverlay` + `use-external-names: false`

Section 10 listed this as unverified. **Measured: both modes pass on unmodified
`next`** (scan exit 0, include tree produced). So on `next` the
`CASFileWithFixedStatus` branch in `RedirectingFileSystem::openFileForRead` is
**live and load-bearing**, not dead code. Removing it without replacing the
mechanism would have broken a working configuration. This is the single data
point the brief asked for, and it inverts the framing: on `next` this work
*removes a workaround*, it does not *fix a live crash*.

### C2. The replay-side overlay bug (section 5) does not exist on `next`

Section 5 claimed `configureInvocationForCaching` "does not strip
`HeaderSearchOpts::VFSOverlayFiles`, so the replay command line still carries
`-ivfsoverlay`", producing
`fatal error: virtual filesystem overlay file ... not found`.

**This is wrong on `next`.** `configureInvocationForCaching`
(`clang/lib/DependencyScanning/ScanAndUpdateArgs.cpp`, `CachingInputKind::IncludeTree`)
does

```c++
HeaderSearchOptions OriginalHSOpts;
std::swap(HSOpts, OriginalHSOpts);
```

and restores only `Sysroot`, `ResourceDir`, `PrebuiltModuleFiles`,
`ModuleFormat` and `ModulesSkipDiagnosticOptions`. `VFSOverlayFiles` is **not**
restored, so `-ivfsoverlay` is stripped. Verified: `clang -cc1depscan ... -ivfsoverlay ...`
produces a response file with **zero** occurrences of `ivfsoverlay`, and
`clang @rsp` replays and compiles successfully in *both* `use-external-names`
modes.

The memo's reproducer B only failed because it passed `-ivfsoverlay` to the
replay `-cc1` by hand, which the real scan->replay flow never does.

**Consequences:** recommendation 9.6 (add `VFSOverlayFiles` to
`finishIncludeTree`) is unjustified and was **implemented, tested, and then
reverted** — adding the YAML to the file list changes include-tree IDs and cache
keys to record a file replay never reads. Section 5's headline evidence for the
"replay file set is a strict superset" claim is also void; that claim now rests
only on the hand-maintained list in `finishIncludeTree` (profile files,
`SDKSettings.json`), which is a weaker argument than the memo presents.

### C3. The root cause, stated precisely — and why *both* path and RTTI fail together

Section 3's mechanism was right about `FileWithFixedStatus` but incomplete about
*why* `use-external-names: true` escapes. In `next`,
`RedirectingFileSystem::openFileForRead` wraps the file in `FileWithFixedStatus`
**unconditionally** once a remap entry matches — it does not depend on
`use-external-names`. The real discriminator is *which name the second lookup
uses*:

| | Name on `FileEntryRef` | Second lookup for the ref | Decorated? |
|---|---|---|---|
| `use-external-names: true` | the **external** path (`real/actual_header.h`) | the overlay does **not** map that path, so it falls through straight to the scanning FS | **no** -> `dyn_cast` works, by-path works |
| `use-external-names: false` | the **virtual** path (`vdir/virtual_header.h`) | the overlay **does** map it, so the file is re-decorated | **yes** -> `dyn_cast` fails, and the path is invisible below the overlay |

Direct evidence from the scan output on unmodified `next`: with
`use-external-names: true` the include tree records
`/tmp/.../real/actual_header.h`; with `false` it records
`/tmp/.../vdir/virtual_header.h`. The recorded name *is* the name used for the
ref query.

This unifies the two failure theories into one root cause, and the consequence is
the key design argument:

> **Recovering the ref by `dyn_cast`ing through decorators and re-resolving the
> ref by path fail in exactly the same configurations**, because both depend on
> the *reported name* round-tripping back through the same VFS stack to the same
> bytes. Whenever a VFS layer remaps a name, the round trip is broken in both
> ways at once: the file gets decorated (destroying RTTI identity) *and* the
> reported name does not exist below that layer (destroying path identity).

Only **content identity** is immune to both, because in a content-addressed store
the reference is a pure function of the bytes.

## What was implemented

### LLVM CAS layer

`class CASBackedFile` is **deleted**. `CASBackedFileSystem` keeps its
`RTTIExtends<CASBackedFileSystem, vfs::FileSystem>` root (already upstream) and
gains a single pure-virtual **content** primitive:

```c++
virtual Expected<std::pair<std::unique_ptr<MemoryBuffer>, cas::ObjectRef>>
getBufferAndObjectRefForFile(const Twine &Name, int64_t FileSize = -1,
                             bool RequiresNullTerminator = true,
                             bool IsVolatile = false, bool IsText = true) = 0;
```

The reference is produced by the very read that produces the buffer, so a caller
that wants both pays for one open and never re-derives anything.
`getObjectRefForFileContent(const Twine &)` survives as a **non-virtual**
convenience over it (used by `llvm-cas`), carrying a doc comment warning not to
feed it a path taken from the top of a VFS stack. `openCASBackedFileForRead` and
the `openFileForRead` laundering override are gone; each implementation now
provides a plain `openFileForRead` returning a plain `vfs::File`.

Five duplicated wrapper classes collapsed into their plain counterparts:
`CASFileSystem::VFSFile`, `CachingOnDiskFileSystemImpl::VFSFile`,
`CASProvidingFile` (deleted outright), `IncludeTreeFileSystem::IncludeTreeFile`,
`DepScanCASFile` (deleted outright), plus `CASFileWithFixedStatus` in Support.

One deliberate behaviour change: `CASProvidingFileSystem` no longer ingests into
the CAS as a side effect of a plain `openFileForRead`; it ingests when a
reference is actually requested. This is strictly less hashing. Where the
underlying filesystem is itself CAS-backed, `getBufferAndObjectRefForFile`
chains into the underlying one so nested stores are still populated — this is
what `CASProvidingFileSystemTest.WithCASSupportingFS` pins, and it caught the
omission when I first wrote it.

### Scanner — reference capture

`DependencyScanningWorkerFilesystem::readFile` is the one place where the file is
opened and the CAS filesystem is statically known, so that is where the
reference is captured:

```c++
if (auto *CASFS = dyn_cast<llvm::cas::CASBackedFileSystem>(&FS)) {
  auto MaybeBufferAndRef = CASFS->getBufferAndObjectRefForFile(Filename);
  ...
}
```

This is a `vfs::FileSystem`-level `dyn_cast` (no core-LLVM change) on the
filesystem the cache *owns*, with static types. It fixes the trap the coordinator
flagged: in `next` this field was previously populated by the same fragile
`dyn_cast<CASBackedFile>` on an opened file, so merely *reading* refs out of the
cache would have relocated the fragility rather than removed it.

Cost: one extra `status()` call per cache miss (the primitive returns buffer+ref
but not status). That is once per unique file per shared cache, against a read
that already happens. Not measured as significant.

### Consumer — `IncludeTreeBuilder`

`FileManager::getObjectRefForFileContent` is deleted. Its two callers now go
through one new helper:

```c++
Expected<cas::ObjectRef> IncludeTreeBuilder::getObjectRefForFileContents(
    FileManager &FM, FileEntryRef FE,
    std::optional<llvm::MemoryBufferRef> Contents) {
  if (Contents)
    return DB.storeFromString({}, Contents->getBuffer());
  auto Buffer = FM.getBufferForFile(FE, /*isVolatile=*/false,
                                    /*RequiresNullTerminator=*/true,
                                    /*MaybeLimit=*/std::nullopt,
                                    /*IsText=*/false);
  if (!Buffer)
    return llvm::createFileError(FE.getName(), Buffer.getError());
  return DB.storeFromString({}, (*Buffer)->getBuffer());
}
```

- For every file the preprocessor saw (the bulk — all headers and the main
  file), `addToFileList` is handed
  `FI.getContentCache().getBufferIfLoaded()` — **the buffer the compilation is
  actually using**. This is correct by construction and is also *more* correct
  than before for content overrides (`overrideFileContents`, `-remap-file`),
  where the old path-based query re-opened the file through the VFS and would
  have picked up the stale on-disk bytes (memo section 4, listed there as an
  unverified pre-existing bug — this design removes it rather than inheriting it).
- For files only ever named by a path (`NoSanitizeFiles`, the three profile
  paths, `SDKSettings.json`, the implicit PCH), the contents are read through
  `FileManager::getBufferForFile`, i.e. through the **whole** VFS stack, so every
  overlay and override is honoured exactly as the compilation would see it. This
  is not a regression: the old code also opened and (via
  `CASProvidingFileSystem`) hashed these files on this path.

`addToFileList` is therefore now **total** on the hot path: it cannot fail with
`std::errc::not_supported`, and the `err_cas_depscan_failed` diagnostic is no
longer reachable for "no CAS reference for this path".

**Performance:** the one case that costs more than before is a file whose
contents the compilation loaded *and* whose reference the scanner had already
cached — there the bytes are now hashed a second time (BLAKE3, once per
include-tree file). This was a deliberate trade for a design with no path
ambiguity and no new plumbing. I did **not** measure it. If it matters, the fix
is local and needs no interface change: give
`DependencyScanningWorkerFilesystem` a content-identity lookup
(`buffer data pointer -> ObjectRef`, exact because `DepScanFile` hands out a
non-owning buffer over the shared `CachedFileContents::Original`) and consult it
before falling back to `storeFromString`. **Unverified / not implemented.**

## The decisive experiment: path-based step 4 is a regression

I implemented the memo's step 4 literally — ask the `CASBackedFileSystem` found
in the scan instance's VFS stack for `FE.getName()` — built it, and ran the
discriminating pair. Result:

| Design | `use-external-names: true` | `use-external-names: false` |
|---|---|---|
| unmodified `next` (RTTI + Support inversion) | pass, root `19937f36...` | pass, root `279edec...` |
| **path-based** (memo step 4) | pass, root `19937f36...` | **FAIL** |
| **content-based** (implemented) | pass, root `19937f36...` | pass, root `279edec...` |

The path-based failure, via `cc1depscan` which reports it cleanly:

```
fatal error: CAS-based dependency scan failed: No such file or directory
fatal error: CAS-based dependency scan failed: failed to get include-tree
```

The cause is exactly as predicted in C3: the name on the `FileEntryRef` is
`<dir>/vdir/virtual_header.h`, and that path exists in **no filesystem below the
overlay** — verified directly (`test -e` on the virtual path fails; the `vdir`
directory on disk is empty). The scanner's own cache is keyed by filename too,
so sourcing refs from it by path inherits the identical defect.

So the memo's own recommendation 9.4 would have **broken a configuration that
works on `next` today**. Content identity is what makes Option D viable here.

Both halves of this experiment are preserved as permanent tests (see below), so
the conclusion is re-checkable rather than a one-off measurement.

## Every consumer in `next`, and how each was handled

Exhaustive grep for `CASBackedFile` (excluding `CASBackedFileSystem`) over
`llvm/` and `clang/`. **The brief expected more consumers in `next` than in the
series; there are in fact almost exactly the same ones.** Specifically there are
**no** consumers in MCCAS, `ASTReader`/`ASTWriter`/`ModuleManager`,
`ModuleDepCollector`, libclang (`CCAS.cpp`, `CDependencies.cpp`), `cc1depscand`,
IndexStore, or `clang-cas-test` — grep returns zero hits in all of those. The
only `next`-specific consumer is the Support-library workaround itself.

| Site | Role | Handling |
|---|---|---|
| `llvm/include/llvm/CAS/CASFileSystem.h:22` | `class CASBackedFile` | **deleted** |
| `llvm/lib/CAS/CASFileSystem.cpp:17,126,338` | `VFSFile`, `openCASBackedFileForRead` | plain `vfs::File`; new `getBufferAndObjectRefForFile`; shared `lookupFileEntry` helper |
| `llvm/lib/CAS/CASProvidingFileSystem.cpp:20,86` | `CASProvidingFile`, open | wrapper **deleted**; primitive returns buffer+ref; chains into underlying CAS FS |
| `llvm/lib/CAS/CachingOnDiskFileSystem.cpp:225,485` + `.h:133-139` | `VFSFile`, `openCASBackedFileForReadImpl` | plain `vfs::File`; the `Impl`/`final` indirection pair **deleted** |
| `llvm/lib/Support/VirtualFileSystem.cpp:24-25,2591,2691,3086-3088` | `#include` CAS, `CASFileWithFixedStatus`, `dyn_cast` branch, `CASBackedFile::ID` | **all deleted** — zero CAS references remain |
| `llvm/include/llvm/Support/VirtualFileSystem.h:119` | `File : RTTIExtends<File, RTTIRoot>` | reverted to plain `class File`; `const char File::ID` definition removed |
| `clang/lib/Basic/FileManager.cpp:564,590,614` + `.h:277,283,295,301` | 3 `dyn_cast` sites, `CASContents` out-param x3, `getObjectRefForFileContent` | **all deleted**; `getBufferForFileImpl` restored to the upstream `FS->getBufferForFile(...)` form; both CAS `#include`s dropped |
| `clang/lib/CAS/IncludeTree.cpp:998,1019,1064` | `IncludeTreeFile`, open | plain `vfs::File`; new `getBufferAndObjectRefForFile` from the already-stored `ContentsRef` |
| `clang/lib/DependencyScanning/DependencyScanningFilesystem.cpp:39,443` | `readFile` cast, `DepScanCASFile` | cast replaced by FS-level `dyn_cast` + `getBufferAndObjectRefForFile`; `DepScanCASFile` **deleted**, `DepScanFile::create` has a single branch |
| `clang/lib/DependencyScanning/IncludeTreeActionController.cpp:721,932` | 2 callers of the `FileManager` API | both routed through the new content-based `getObjectRefForFileContents`; PCH path now resolves a `FileEntryRef` first |
| `llvm/tools/llvm-cas/llvm-cas.cpp:627` | `cast<CASBackedFileSystem>` + by-path | **unchanged** — already FS-level, and a by-path query is correct there (single CAS FS, no stack) |
| `llvm/unittests/CAS/CASProvidingFileSystemTest.cpp:33,83` | 2 sites | retargeted to `openFileForRead` / `getObjectRefForFileContent` |
| `clang/unittests/Basic/FileManagerTest.cpp:545-598` | `CASProvider` test | rewritten: FileManager serves buffers, the ref comes from the FS |
| `clang/unittests/Tooling/DependencyScannerTest.cpp:265,289` | 2 sites | retargeted to `getOrCreateFileSystemEntry` + `EntryRef::getObjectRefForContent` — i.e. they now test the *cache*, which is the thing that matters |

Also changed: `clang/tools/clang-scan-deps/ClangScanDeps.cpp` error handling
(see below). MCCAS was not touched and still builds.

## Adversarial review: what this change could break

Each case the brief listed, assessed against the content-based design.

| Case | Verdict |
|---|---|
| `-ivfsoverlay`, `use-external-names: true` | **No change.** Root ID identical to baseline. Lit-tested (pre-existing `include-tree-vfs-overlay.c`). |
| `-ivfsoverlay`, `use-external-names: false` | **No change.** Root ID identical to baseline. New lit test. |
| Same path -> different content through overlay layers | **Improved.** The ref follows the bytes, so a remap onto a different file cannot alias. `include-tree-vfs-overlay.c` exercises a `header1.h -> header2.h -> header3.h` remap chain and passes. |
| `overrideFileContents` / `-remap-file` / in-memory buffers | **Improved.** The buffer the compilation uses *is* the overridden one. The old by-path query re-opened through the VFS and would return the stale on-disk ref (memo section 4 lists this as a pre-existing bug; it is now structurally impossible on the loaded-contents path). |
| Non-canonical paths, `..`, trailing slashes, symlinks | **No new exposure.** The path is still used only as the include-tree *entry name* (unchanged code); it no longer participates in deriving the ref, so path aliasing can no longer produce a wrong ref. |
| `-working-directory`-relative paths | **No change.** `addToFileList` still applies `FixupRelativePath` to the recorded name. `include-tree-working-directory.c` passes. |
| Case-insensitive filesystem collisions | **Improved** — content key is case-blind by construction. `include-tree-vfs-overlay.c` uses `case-sensitive: false` and passes. |
| File modified or deleted between open and query | **Improved.** For loaded contents there is no second query at all. |
| Files opened by file descriptor (`FileEntry::File`) | **No longer consulted** for refs; the `dyn_cast` on `Entry->File` is deleted. |
| `-ivfsstatcache` | **Not affected**: it is a stat cache and does not decorate `vfs::File` (no `vfs::File` subclass in its implementation). |
| Header maps | Not in the include tree before or after; `HeaderMap` reads via `getBufferForFile`, whose CAS out-param was dead. **No change.** |
| Module `.pcm` reads (`ASTReader`/`ASTWriter`/`ModuleManager`) | **No change** — they never passed the `CASContents` out-param. Confirmed by grep: zero non-test users in `next`. |
| Implicit PCH | Now resolves a `FileEntryRef` then reads contents through the whole VFS stack. `fcas-include-tree-with-pch.c`, `chained-pch.c`, `clang-cache-pch.c` pass. |
| MCCAS | **Untouched**; zero consumers of these APIs. Builds and its tests pass. |

### Real issues I found in my own change (both fixed)

1. **`CASProvidingFileSystem` stopped ingesting on plain `openFileForRead`.** My
   first version routed the primitive through `FS->getBufferForFile`, which for a
   *nested* CAS-providing filesystem skipped the inner store.
   `CASProvidingFileSystemTest.WithCASSupportingFS` failed and caught it. Fixed
   by chaining into the underlying `CASBackedFileSystem` when present. The
   remaining intentional difference — no ingestion unless a ref is requested —
   is strictly less work and no consumer depends on the side effect.
2. **I implemented and then reverted memo recommendation 9.6** (adding
   `VFSOverlayFiles` to the include-tree file list) once I established that
   `-ivfsoverlay` is stripped from replay commands. See C2.

### Latent hazard found but not reproduced

`PrefixMappingFile` (`llvm/lib/Support/PrefixMappingFileSystem.cpp`) is a
`vfs::File` decorator that `next`'s `RedirectingFileSystem` workaround does
**not** cover, and `IncludeTreeActionController::initialize` installs that
filesystem on the scan instance's `FileManager` whenever prefix mapping is
active together with `-include`, an implicit PCH, or modules. It wraps the file
**only when the path was actually remapped** (`if (DidMap && F)`) — the same
conditional-decoration shape as the overlay case.

I could **not** construct a failing input on unmodified `next`: the mapper
installed there is a *reverse* mapper, and `PrefixMappingFileSystem::status`
reports the underlying real name, so the name that reaches the ref query is the
unmapped real path and escapes decoration — structurally the same accident that
saves `use-external-names: true`. So this is a **latent** hole, not a
demonstrated bug. **Unverified.** It is moot after this change, since no
`vfs::File` cast remains. I also removed a dead
`#include "llvm/CAS/CASReference.h"` from that file, a leftover of the same
coupling.

## Tests added, and their load-bearing proof

Every new test was inverted, confirmed to fail, restored, and confirmed to pass.

### 1. `clang/test/ClangScanDeps/include-tree-vfs-overlay-virtual-names.c` (new)

`-ivfsoverlay` + `use-external-names: false`, scan + replay + compile. Pins the
case the Support-library workaround was covering, which had **no test** before
(the pre-existing `include-tree-vfs-overlay.c` uses the default
`use-external-names: true`).

**Load-bearing — proved by reimplementing the rejected design, not by editing an
expectation.** With the memo's path-based step 4 in place the test **FAILS**
while `include-tree-vfs-overlay.c` still passes; reverting to content-based
makes it pass again. That is the strongest possible demonstration: the test
discriminates exactly the design decision it exists to protect.

### 2. `CASProvidingFileSystemTest.RefIsKeyedOnContentsNotPath` (new)

Builds a `RedirectingFileSystem` (`use-external-names: false`) over a
CAS-providing filesystem — the stack a scan actually builds — and pins three
things: the CAS filesystem is **not** reachable by `dyn_cast` from the top of
the stack; the name the stack reports for the file (`vdir/virtual.h`) **fails**
when handed to the CAS filesystem; and the ref computed from the bytes read
through the stack **equals** the ref the CAS filesystem assigned to the file it
did see.

**Load-bearing:** inverting the `Failed()` expectation on the by-path query to
`Succeeded()` makes it FAIL; restoring makes it PASS. This test is also valid
*before* the change (it uses only APIs present on unmodified `next`), so it can
land first and document the constraint independently.

### 3. Retargeted, not deleted

`DependencyScannerTest.DepScanFSWithCASProvider` previously asserted the ref was
reachable from the opened file. It now asserts it is reachable from the *cache*
(`getOrCreateFileSystemEntry` + `EntryRef::getObjectRefForContent`), including
across a fresh `DependencyScanningWorkerFilesystem` instance sharing the
service cache. That is the property that actually matters and it now also covers
the fixed cache-population path. `FileManagerTest.CASProvider` was rewritten to
check that a `FileManager` over a CAS-providing FS still serves buffers while
the ref comes from the filesystem.

### Not tested, deliberately

The `clang-scan-deps` `handleAllErrors` fix has **no regression test**: on
`next` every error that reaches that function derives from `StringError`, so I
could not construct a live abort, and I would not write a test asserting a
behaviour I cannot trigger. Stated as a latent hazard in the commit message.

## Test results: baseline vs. after

Both runs on the same machine, `build/release`, assertions on. The baseline was
measured in a **separate pristine worktree** (`~/Code/swift-llvm-base`, detached
at the same commit) so that nothing was attributed to this change by accident.
An earlier in-place attempt was discarded because concurrent edits corrupted it,
and a second branch `check-clang` run was discarded because it raced with a
relink (460 spurious failures); only the clean runs are reported.

| Suite | Baseline (`8af41bd656fb`) | After (Option D) | Delta |
|---|---|---|---|
| `check-llvm` | 58818 passed, **7 failed**, 79 XFAIL | 58819 passed, **7 failed**, 79 XFAIL | +1 passed (new unit test) |
| `check-clang` | 54875 passed, **9 failed**, 54 XFAIL | 54876 passed, **9 failed**, 54 XFAIL | +1 passed (new lit test) |
| `CASTests` (unit) | — | 133/133 passed | — |
| `AllClangUnitTests` | — | exit 0; 27369 passed, 7 skipped, 0 failed | — |

**The failure sets are byte-identical in both suites** (verified by `diff` of the
sorted `FAIL:` lines). No test regressed, and nothing that failed before passes
now by accident.

The 7 pre-existing `check-llvm` failures:
`Transforms/Coroutines/coro-retcon-frame-auto-upgrade-test.ll` plus 6
`SupportTests` shards, which reduce to `FileSystemTest.permissions` and five
`raw_socket_streamTest` cases — environmental (socket/permission sandboxing),
unrelated to VFS or CAS.

The 9 pre-existing `check-clang` failures are all in `clang/test/CAS/` and are
all the `cc1depscand` daemon tests (`depscan*`, `fdepscan*`, `daemon-cwd`,
`validate-once`, `cache-launcher-scan-diagnostics`). Every one fails with
`clang -cc1depscand: cannot create listener: Operation not permitted Bind error`
— the sandbox forbids binding a unix domain socket. Also environmental.

Targeted suites (`llvm/test/CAS`, `clang/test/CAS`, `clang/test/ClangScanDeps`)
are subsets of the above and show no change beyond the added test.

**Caveat:** because the 9 daemon tests cannot run in this environment, the
`cc1depscand` path is covered only by my manual `clang -cc1depscan` invocations
(which did exercise scan + replay in both overlay modes, and which is how the
`-ivfsoverlay`-is-stripped finding was established). **The daemon-mediated path
is not covered by an automated run here.**

## Revised recommendations

Replacing section 9 where it differs:

1. **Keep** 9.1 (delete the dead `CASContents` out-param, restore
   `getBufferForFileImpl`). Done; verified it has no non-test caller in `next`
   either, and that the full `check-clang` passes without it.
2. **Keep** 9.2 (delete `CASBackedFile`, revert `vfs::File`), but make
   **`getBufferAndObjectRefForFile` the pure-virtual primitive**, not the by-path
   `getObjectRefForFileContent`. The buffer+ref pair is the content-oriented
   shape; the by-path query should remain a non-virtual convenience, documented
   as unsafe for paths taken from the top of a VFS stack.
3. **Keep** 9.3 in spirit (`DependencyScanningWorkerFilesystem` gets a
   `dyn_cast<CASBackedFileSystem>` handle and uses the statically typed
   primitive in `readFile`) — and note this is **required**, not optional: it is
   what stops the fragility from relocating into the cache.
4. **Replace** 9.4. Do **not** have the include-tree builder ask anything by
   path — not the bottom filesystem (perf regression, per section 4) and not the
   scanner's cache either (**correctness** regression, measured above, because
   that cache is also path-keyed). Derive the ref from the contents the
   compilation read.
5. **Keep** 9.5 but downgrade it: the unchecked-error pattern in
   `clang-scan-deps` is a real latent hazard and worth fixing, but on `next` it
   is not currently reachable and the `not_supported` error it used to surface no
   longer exists.
6. **Drop** 9.6 entirely. See C2.

Also worth revisiting in the memo itself: section 5's conclusion about (B) leans
on the overlay-YAML replay example, which is void (C2). The "replay file set is a
strict superset" claim is not demonstrated by anything I could reproduce.

## What I could not verify

- **Performance.** I did not measure the cost of the extra hash per
  include-tree file, nor the extra `status()` per cache miss. Both are
  reasoned-about only. The claim that this is an acceptable trade is a
  **judgement, not a measurement.**
- **The `PrefixMappingFile` hazard.** Identified by code reading; I could not
  construct a failing input on unmodified `next` (see above). Moot after this
  change, but the claim "this was a live bug" is **not** supported.
- **`cc1depscand` daemon mode under automated test.** Blocked by the sandbox;
  see the caveat above.
- **Thread safety.** `createThreadSafeProxyFS` is preserved and unchanged, but I
  did not analyse whether the content-based path changes contention
  characteristics. Nothing new is shared: `storeFromString` was already called
  on this path for non-file buffers and API notes.
- **Windows.** I pass `IsText=false` when reading path-only files so binary
  contents (profile data, PCH) hash verbatim, matching the old behaviour. Not
  tested on Windows.
- **Whether `getBufferIfLoaded()` is always populated** when `addToFileList` is
  reached from a preprocessor callback. It is in every test exercised here, and
  the code falls back to reading through the `FileManager` when it is not, so a
  miss degrades to the old behaviour rather than failing — but I did not prove
  the invariant.
- I did **not** re-run the MCCAS-specific or IndexStore-specific suites in
  isolation; they are included in `check-clang`, which is clean.

### Targeted suites, measured on both trees

`llvm/test/CAS` + `clang/test/CAS` + `clang/test/ClangScanDeps`:

| | Discovered | Passed | Failed | XFAIL |
|---|---|---|---|---|
| Baseline | 293 | 277 | 9 | 1 |
| After | **294** | **278** | 9 | 1 |

Same 9 failures (the sandboxed `cc1depscand` daemon tests), one more test, one
more pass.

## Environment note (not caused by this work, but worth knowing)

During this session `~/Code/swift-llvm`'s own detached HEAD moved from
`20010620e83f` to `d9a40e913845`, and the `M apple-ci/amtool` modification that
was present at the start is no longer in its working tree.

I did not do this. The only command I ran in that directory was
`git fetch origin next` (plus read-only `git worktree list` / `log` /
`rev-parse`); `fetch` cannot move HEAD or create a commit. Its reflog attributes
the change to

```
8af41bd656fb HEAD@{2026-11-05 05:10:59}: checkout: moving from 20010620e83f to FETCH_HEAD
d9a40e913845 HEAD@{2026-11-05 05:11:52}: commit (merge): Merge commit 'a175e994979a' from llvm.org/main into next
```

which is exactly the apple-llvm automerger's pattern and matches the pre-existing
entries from Nov 3 (`refs/am/changes/...` refs confirm that tooling runs in this
repo). It appears the automerger ran concurrently with this session.

`20010620e83f` is intact and reachable. The fate of the uncommitted
`apple-ci/amtool` edit is unknown to me — **worth checking** before assuming it
is safe. All of my own work is confined to `~/Code/swift-llvm-rtti`
(branch `cas/vfs-file-rtti-option-d`) and the throwaway baseline worktree
`~/Code/swift-llvm-base`, and I destroyed no stash (the one stash entry I
created, `stash@{0} optd-llvm-part`, was applied with `git stash apply` and
deliberately left in place).
