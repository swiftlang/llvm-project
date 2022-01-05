# Things to try on this branch

## LLVMCAS library

Play with `llvm-cas`. E.g., print a tree:

```
% ninja llvm-cas
% llvm-cas --cas $TMPDIR/casfs.default --ls-tree <tree-id>
```
Merge directories:
```
% llvm-cas --cas $TMPDIR/casfs.default --merge <tree-id> /path/to/dir
```

TODO: add demos for plugins once there are some.

## Building against a CAS: Clang compiler caching

### Clang command-line options

Some of these are currently `-cc1`-only. Some should stay that way, others just
need to be "fixed". Some are driver-only (and should remain so).

- `-greproducible`: Make debug info more reproducible. Both `-cc1` and linker.
    - Avoids writing out Clang's Git revision (maybe it should avoid the
      version altogether).
    - Tells the linker not to write object file timestamps in the debug map.
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
    - `-fdepscan=auto` is the current alias for `-fdepscan`. It is the same
      as `-fdepscan=daemon` if it seems profitible, otherwise `=inline`.
    - `-fdepscan=daemon` always makes a daemon. It may allow shared state:
        - `-fdepscan-share-parent`: share state based on the PID of the parent
          process.
        - `-fdepscan-share-parent=ninja`: share state based on the PID of the
          parent process if it's called `ninja`.
        - `-fdepscan-share=ninja`: share state based on the PID of the
          closest `ninja` in the process ancestors.
        - `-fdepscan-share-stop=cmake`: do not share state if `cmake` is the
          parent process, or is a closer process ancestor than the command to
          share under. This protects against shared state in configuration
          files.
    - `-fdepscan=inline`: never daemonize. This is much slower.
    - `-fdepscan=off`: the default!, but can be added to the end of a
      command-line.
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

This branch's CMake option `-DLLVM_ENABLE_EXPERIMENTAL_DEPSCAN_TABLEGEN=ON`
configures everything (doesn't matter what toolchain you're using). 

### Construct a toolchain with CAS support

Pretty hacky right now (can this be trimmed down?).

```
% TOOLCHAIN_SRC="$(dirname $(dirname $(dirname $(xcrun -find clang))))"
% TOOLCHAIN=/absolute/path/to/new/toolchain
% ditto "$TOOLCHAIN_SRC" "$TOOLCHAIN"

% STAGE1BUILD=path/to/stage1/build
% (cd "$STAGE1BUILD" &&
   cmake -DCMAKE_INSTALL_PREFIX="$TOOLCHAIN/usr" ../llvm &&
   ninja install-clang install-clang-resource-headers install-LTO)
```
Optionally also install experimental libtool and linker support:
```
% (cd "$STAGE1BUILD" && ninja install-lld install-llvm-libtool-darwin)
```

#### Caveats to investigate / fix

- At some point there was a problem in the assembler. May still be there. Have
  been avoiding it for a long time with:
        -DCMAKE_ASM_COMPILER=$(xcrun -find clang)
  problem (can't remember actual problem...).
- Passing `-fdepscan-prefix-map` changes stderr to use the remapped locations.
  This makes stderr cacheable, but it also seems to interfere with CMake's
  platform detection. Do not pass these flags to `-CMAKE_{C,CXX}_FLAGS`.
    - Perhaps the implementation should generate and cache serialized
      diagnostics with the map in place, but send to stderr the deserialized
      diagnostics with an inverse map. Probably needs some maneuvering in the
      DiagnosticsEngine.
- `-fdepscan-prefix-map` changes the debug info to point at a canonical
  location. The debugger needs to be told the "real" location of the source
  files or else it can't find them. For LLDB, these options can help:

       (lldb) settings append target.source-map /<key> /<value>
       (lldb) settings show target.source-map

  There's no tooling yet for doing this automatically in the build.
- `-fcas-token-cache` is pretty experimental and fragile, since the layering
  is a bit of a hack. It particularly interferes with errors encountered during
  preprocessing.

### Caching with just-built toolchain

#### LLVM project CMake configuration

For building a CAS-aware branch (i.e., this one!), there are some extra CMake
options available.

- `-DLLVM_ENABLE_EXPERIMENTAL_DEPSCAN=ON`: turn on `-fdepscan` and all the
  relevant `-fdepscan-*` options.
    - `-DLLVM_DEPSCAN_MODE=...` pass through to `-fdepscan-mode`.
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
     -DCMAKE_C_COMPILER=$CLANG                      \
     -DCMAKE_CXX_COMPILER=${CLANG}++                \
     -DCMAKE_ASM_COMPILER=$(xcrun -find clang)      \
     -DLLVM_ENABLE_EXPERIMENTAL_DEPSCAN_TABLEGEN=ON \
     -DLLVM_ENABLE_EXPERIMENTAL_DEPSCAN=ON          \
     -DLLVM_ENABLE_EXPERIMENTAL_CAS_TOKEN_CACHE=ON  \
     ../llvm &&
   ninja)
```

Using `-DCMAKE_BUILD_TYPE=Release` speeds up the cached builds significantly,
mainly by speeding up the linker and reducing I/O when writing out the smaller
`.o` files during cached compilation. It also speeds up tablegen, of course.

There are also some CMake flags for turning on basic linker integration:

- `-DLLVM_CAS_ENABLE_CASID_OBJECT_OUTPUTS=ON`: write lightweight Clang outputs,
  where the `.o` written to disk just contains the CAS object identifier. Only
  supported by "lld/machO" (see below). Not supported by LLDB or dsymutil.
    - `-DCMAKE_LIBTOOL="$TOOLCHAIN/usr/bin/llvm-libtool-darwin"`: point at a
      libtool that understands these object files.
    - `-DLLVM_USE_LINKER="$TOOLCHAIN/usr/bin/ld64.lld"`: point at a linker that
      understands these object files.
- `-DLLVM_ENABLE_EXPERIMENTAL_LINKER_RESULT_CACHE=ON`: use a result cache in
  the linker.

#### Manual configuration for other branches

If the branch being built isn't CAS-aware, you should be able to add the
options directly.

```
% CLANG="$TOOLCHAIN"/usr/bin/clang
% STAGE2BUILD=path/to/stage2/build
% SDK="$(xcrun -show-sdk-path)"
% rm -rf "$STAGE2BUILD"
% mkdir -p "$STAGE2BUILD"
% CLANGFLAGS=(
     -greproducible
     -fdepscan -fdepscan-share-parent=ninja -fdepscan-share-stop=cmake
     -fdepscan-prefix-map-sdk=/^sdk
     -fdepscan-prefix-map-toolchain=/^toolchain
     -fdepscan-prefix-map=$(cd $STAGE2BUILD && pwd)=/^build
     -fdepscan-prefix-map=$PWD=/^source
     -Xclang
     -fcas-token-cache
  )
% (cd "$STAGE2BUILD" &&
   cmake -G Ninja                                               \
     -DLLVM_ENABLE_PROJECTS="clang"                             \
     -DCMAKE_C_COMPILER=$CLANG                                  \
     -DCMAKE_CXX_COMPILER=${CLANG}++                            \
     -DCMAKE_ASM_COMPILER=$(xcrun -find clang)                  \
     -DCMAKE_{C,CXX}_FLAGS="$CLANGFLAGS[*]"                     \
     -DCMAKE_{EXE,SHARED,MODULE}_LINKER_FLAGS="-greproducible"  \
     ../llvm &&
   ninja)
```

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

#### Try debugging just built compiler

CMake on a CAS-aware branch creates a .lldbinit file in the build directory
which helps to map the canonicalized path back to the path on disk:

```
% lldb -- $COMMAND

(lldb) command source $BUILD_DIR/.lldbinit
```

or source from lldb commandline:

```
% lldb --source $BUILD_DIR/.lldbinit -- $COMMAND
```

or if your CWD is in `$BUILD_DIR`, just run:

```
% lldb --local-lldbinit -- $COMMAND
```

You can also manually do that by using `settings set target.source-map` command.

#### Build swift compiler

In order to build swift compiler from `main` branch which is not CAS aware, do:

```
% CCC_OVERRIDE_OPTIONS="+-fdepscan +-fdepscan-share-parent=ninja +-fdepscan-share-stop=cmake" \
  ./utils/build-script --host-cc $CLANG --host-cxx ${CLANG}++ $OTHER_FLAGS
```

Or if you use the pre-built cas-aware toolchain, you can do:

```
# only need to run the first command once, maybe include that in the future toolchain?
% (cd $TOOLCHAIN && ln -s clang cc)
% CCC_OVERRIDE_OPTIONS="+-fdepscan +-fdepscan-share-parent=ninja +-fdepscan-share-stop=cmake"
	TOOLCHAINS="Local Swift Development Snapshot" ./util/build-script $OTHER_FLAGS
```

Or if you are on the cas-aware swift branch, checkout from `experimental/cas/rebranch` branch via:

```
% ./util/update-checkout --scheme experimental/cas/rebranch
```

You can build the scheme with a CAS aware toolchain:

```
% TOOLCHAINS="Local Swift Development Snapshot" ./util/build-script --cas $OTHER_FLAGS
```

Notes about using `CCC_OVERRIDE_OPTIONS` vs `--cas` option:

* `CCC_OVERRIDE_OPTIONS` to overwrite clang options globally because swift build script on `main` does not provide access to CMake command-line to add `CMAKE_{C,CXX}_FLAGS`. The downside is we can't apply path fixup or enable other caching features (like TableGen). It can also turn on `-fdepscan` in places that lacks test coverage (e.g. within cmake configuration, swift package manager, etc.).
* `--cas` option on `experimental/cas/rebranch` branch will turn on `-fdepscan` on most of the cmake builds, with llvm-project (except compiler-rt) gets the full support with TableGen and token cache.

## CAS.o: CAS-based object format

Play with `llvm-cas-object-format`. E.g., ingest object files from a build
directory.

```
% BUILDDIR="build"
% OBJECTSDIR="build"
% (cd "$BUILDDIR" && ninja llvm-cas-object-format CASObjectFormatsTests) &&
  "$BUILDDIR"/unittests/CASObjectFormats/CASObjectFormatsTests &&
  find "$OBJECTSDIR"/lib/Support -name "*.o" |
  sort >objects-to-ingest &&
  time "$BUILDDIR"/llvm-cas-object-format --cas "$TMPDIR/casfs.default" \
    @objects-to-ingest --object-stats \
    --keep-compact-unwind-alive=false \
    --prefer-indirect-symbol-refs=true
```

Currently, there are two schemas available for CAS-based object format:
`nestedv1` (default) and `flatv1`. FlatV1 schema uses a more direct
serialization of LinkGraph and works for both MachO and ELF. You can try use
`flatv1` schema with following options:

```
  time "$BUILDDIR"/llvm-cas-object-format --cas "$TMPDIR/casfs.default" \
    --ingest-schema=flatv1 --object-stats @objects-to-ingest
```

Some other use flags to use:

* `--just-blobs`: Ingest the object files as blobs into CAS
* `--ingest-cas`: Treat the inputs as CASID for cas tree instead of filename.
* `--debug`, `--debug-ingest`, `--dump`: Useful debug options (some only work
  with assertion build)

## lld/machO

### Using a CAS filesystem

```
% llvm-cas -cas "$TMPDIR/casfs.default" --merge /path/to/dir > casid
% ld64.lld -arch x86_64 -platform_version macos 12 12 --fcas-builtin-path "$TMPDIR/casfs.default" --fcas-fs @casid /path/to/dir/main.o -o a.out
```

### Caching linker invocations

`ld64.lld` can connect to the builtin CAS and cache its linker work:
```
% ld64.lld -arch x86_64 -platform_version macos 12 12 --fcas-builtin-path "$TMPDIR/casfs.default" --fcas-cache-results t.o -o a.out --verbose
```
With `--verbose` (or `LLD_VERBOSE=1` as environment variable) it will output to `stderr`
to indicate whether there was a cache 'hit' or 'miss'.

### Linking CAS.o

`ld64.lld` can read & link `CAS.o` objects:

```
% llvm-cas-object-format --cas "$TMPDIR/casfs.default" t1.o t2.o -silent > casid
% ld64.lld -arch x86_64 -platform_version macos 12 12 --fcas-builtin-path "$TMPDIR/casfs.default" @casid -o a.out
```
