# Things to try on this branch

## LLVMCAS library

Play with `llvm-cas`. E.g., print a tree:

```
% ninja llvm-cas
% llvm-cas --cas $TMPDIR/casfs.default --ls-tree <tree-id>
```

TODO: add demos for plugins once there are some.

## Building against a CAS: Clang compiler caching

### Clang command-line options

Some of these are currently `-cc1`-only. Some should stay that way, others just
need to be "fixed". Some are driver-only (and should remain so).

- `-fcas=builtin`: use the builtin CAS (defaults to in-memory).
    - `-fcas-builtin-path=<path>`: path to on-disk storage for CAS.
    - `-fcas-builtin-path='/^$TMPDIR/<path>'`: "hack" for referencing temp
      directory without the path to the temp directory showing up on the
      command-line. Probably want something better than this...
- `-fcas-fs=<tree>`: use `<tree>` as the root filesystem. This *should* work in
  the driver (causing the driver to read from the same tree), but currently
  only works in `-cc1`.
- `-fcas-token-cache`: perform a raw lex (no preprocessor) for each input file
  as a separate task from the full lex, and cache the result in the CAS. This
  *should* work always, but currently depends on `-fcas-fs`.
- `-fdepscan`: Driver-only. Before running `-cc1`, use the clang dependency
  scanner to find and the dependencies and create a pruned tree in the CAS,
  then run `-cc1` using `-fcas-fs`. This *should* respect `-fcas` if it was
  specified, but for now always uses the builtin CAS at a fixed path. This
  *should* respect `-fcas-fs` and create a pruned/new CAS tree, but for now
  ignores it.
    - `-fdepscan-mode=daemon`: currently the only option... run in a daemon.
    - `-fdepscan-prefix-map=<key>=<value>`: do prefix mapping on the CAS tree
      and the `-cc1` command-line, rewriting paths starting with `<key>` to
      start with `<value>`. Can be used to make the `-cc1` command-line
      reproducible regardless of location of source and build directories.
    - `-fdepscan-prefix-map-sdk=<value>`: auto-detect the SDK (location of
      `/usr/include`, etc.) and map it to `<value>`.
    - `-fdepscan-prefix-map-toolchain=<value>`: auto-detect the toolchain
      (location of `/usr/bin/clang`) and map it to `<value>`.

### TableGen command-line options

TableGen can scan dependencies and cache too. No daemonization. It always uses
the builtin CAS.

- `--depscan`: turn it on.
- `--depscan-prefix-map=<key>=<value>`: same as `-fdepscan-prefix-map` for
  `-cc1` above.

The CMake option `-DLLVM_ENABLE_EXPERIMENTAL_DEPSCAN_TABLEGEN=ON` turns this
on when building the branch (toolchain doesn't matter).

### Construct a toolchain with CAS support

Pretty hacky right now (can this be trimmed down?).

```
% TOOLCHAIN_SRC="$(dirname $(dirname $(dirname $(xcrun -find clang))))"
% TOOLCHAIN=/absolute/path/to/new/toolchain
% ditto "$TOOLCHAIN_SRC" "$TOOLCHAIN"

% STAGE1BUILD=path/to/stage1/build
% (cd "$STAGE1BUILD" &&
   cmake -DCMAKE_INSTALL_PREFIX="$TOOLCHAIN/usr" &&
   ninja install-clang install-clang-resource-headers install-LTO)
```

### Caching with just-built toolchain

#### LLVM project CMake configuration

For building a CAS-aware branch, there are some extra CMake options available.

- `-DLLVM_ENABLE_EXPERIMENTAL_DEPSCAN=ON`: turn on `-fdepscan` and all the
  relevant `-fdepscan-prefix-map*` options.
    - `-DLLVM_DEPSCAN_MODE=daemon` pass through to `-fdepscan-mode`.
- `-DLLVM_ENABLE_EXPERIMENTAL_DEPSCAN_TABLEGEN=ON`: as above, turns it on for
  the just-built tablegen.
- `-DLLVM_ENABLE_EXPERIMENTAL_CAS_TOKEN_CACHE=ON`: turn on `-fcas-token-cache`.

```
% CLANG="$TOOLCHAIN"/usr/bin/clang
% STAGE2BUILD=path/to/stage2/build
% SDK="$(xcrun -show-sdk-path)"
% rm -rf "$STAGE2BUILD"
% mkdir -p "$STAGE2BUILD"
% (cd "$STAGE2BUILD" &&
   cmake -G Ninja                                   \
     -DLLVM_ENABLE_PROJECTS="clang"                 \
     -DCMAKE_OSX_SYSROOT="$SDK"                     \
     -DCMAKE_C_COMPILER=$CLANG                      \
     -DCMAKE_CXX_COMPILER=${CLANG}++                \
     -DCMAKE_ASM_COMPILER=$(xcrun -find clang)      \
     -DLLVM_ENABLE_LIBCXX=ON                        \
     -DLLVM_ENABLE_EXPERIMENTAL_DEPSCAN_TABLEGEN=ON \
     -DLLVM_ENABLE_EXPERIMENTAL_DEPSCAN=ON          \
     -DLLVM_ENABLE_EXPERIMENTAL_CAS_TOKEN_CACHE=ON  \
     -DLLVM_DEPSCAN_MODE=daemon                     \
     ../llvm &&
   ninja)
```

Using `-DCMAKE_BUILD_TYPE=Release` speeds up the cached builds significantly,
mainly by speeding up the linker and reducing I/O when writing out the smaller
`.o` files during cached compilation. It also speeds up tablegen, of course.

#### Manual configuration for other branches

If the branch being built isn't CAS-aware:

```
% CLANG="$TOOLCHAIN"/usr/bin/clang
% STAGE2BUILD=path/to/stage2/build
% SDK="$(xcrun -show-sdk-path)"
% rm -rf "$STAGE2BUILD"
% mkdir -p "$STAGE2BUILD"
% CLANGFLAGS=(
    -fdepscan
    -fdepscan-prefix-map="$PWD=/^source"
    -fdepscan-prefix-map="(cd "$STAGE2BUILD" && pwd)=/^build"
    -fdepscan-prefix-map-sdk=/^sdk
    -fdepscan-prefix-map-toolchain=/^toolchain"
    -Xclang
    -fcas-token-cache
  )
% (cd "$STAGE2BUILD" &&
   cmake -G Ninja                                   \
     -DLLVM_ENABLE_PROJECTS="clang"                 \
     -DCMAKE_OSX_SYSROOT="$SDK"                     \
     -DCMAKE_C_COMPILER=$CLANG                      \
     -DCMAKE_CXX_COMPILER=${CLANG}++                \
     -DCMAKE_ASM_COMPILER=$(xcrun -find clang)      \
     -DLLVM_ENABLE_LIBCXX=ON                        \
     -DCMAKE_{C,CXX}_FLAGS="$CLANGFLAGS[*]"         \
     ../llvm &&
   ninja)
```

###  Build

Notes on current command-line:

- Adds `-DCMAKE_ASM_COMPILER=$(xcrun -find clang)` to work around some assembler
  problem (can't remember actual problem...).

TODO: Investigate / fix.

#### Try touching a header

```
% touch llvm/include/llvm/ADT/StringRef.h
% (cd "$STAGE2BUILD" && ninja)
```

#### Try cleaning

```
% (cd "$STAGE2BUILD" && ninja clean && ninja)
```

#### Try a different build directory

```
% mkdir new-builddir &&
  (cd new-builddir && cmake ... && ninja)
```

#### Try a different source directory

```
% git worktree add new-source-dir --detach experimental/cas/main &&
  cd new-source-dir

% mkdir build &&
  (cd build && cmake ... && ninja)
```

## CAS.o: CAS-based object format

Play with `llvm-cas-object-format`. E.g., ingest object files from a build
directory.

```
% BUILDDIR="build"
% OBJECTSDIR="build"
% (cd "$BUILDDIR" && ninja llvm-cas-object-format CASObjectFormatTests) &&
  "$BUILDDIR"/unittests/ExecutionEngine/CASObjectFormat/CASObjectFormatTests &&
  find "$OBJECTSDIR"/lib/Support -name "*.o" |
  sort >objects-to-ingest &&
  time "$BUILDDIR"/llvm-cas-object-format --cas "$TMPDIR/casfs.default" \
    @objects-to-ingest --object-stats \
    --keep-compact-unwind-alive=false \
    --prefer-indirect-symbol-refs=true
```
