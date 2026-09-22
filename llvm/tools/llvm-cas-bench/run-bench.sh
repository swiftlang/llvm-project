#!/usr/bin/env bash
#===----------------------------------------------------------------------===#
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===#
#
# Builds llvm-cas-bench in Release without assertions and benchmarks all four
# backends against a corpus: size on disk, insert, and traversal on one thread
# and in parallel. See --help.
#
#===----------------------------------------------------------------------===#

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SRC=$(cd "$SCRIPT_DIR/../../.." && pwd)
NCPU=$(getconf _NPROCESSORS_ONLN)

CORPUS=""
BUILD="$SRC/../cas-benchmark-build"
RESULTS=""
RUNS=5
SCRATCH=""
CMAKE_EXTRA=()

usage() {
  cat <<EOF
Usage: $(basename "$0") --corpus DIR [options]

Builds llvm-cas-bench and runs llvmcas, lmdb, sqlite and file against a
corpus, writing logs, JSON and a markdown summary to a results directory.

llvmcas and lmdb run --runs rounds, interleaved, and the summary reports
medians. sqlite and file run once: a full pass of the two takes hours.
Traversal runs on one thread, on half the cores and on all $NCPU.

Options:
  --corpus DIR      Holds lmdb/, the corpus as an LMDB store, and casid.txt,
                    its root CASIDs
  --build DIR       Build directory (default: <llvm-project>/../cas-benchmark-build)
  --results DIR     Results directory (default: <build>/bench-results/<time>)
  --runs N          Rounds for llvmcas and lmdb (default: $RUNS)
  --scratch DIR     Put the stores here instead of on a throwaway APFS volume.
                    Required off macOS.
  --cmake-arg ARG   Extra cmake argument, e.g. -DSQLite3_LIBRARY=...; repeatable
EOF
}

die() {
  echo "error: $*" >&2
  exit 1
}

while [ $# -gt 0 ]; do
  case "$1" in
  --corpus) CORPUS=$2; shift 2 ;;
  --build) BUILD=$2; shift 2 ;;
  --results) RESULTS=$2; shift 2 ;;
  --runs) RUNS=$2; shift 2 ;;
  --scratch) SCRATCH=$2; shift 2 ;;
  --cmake-arg) CMAKE_EXTRA+=("$2"); shift 2 ;;
  -h | --help) usage; exit 0 ;;
  *) usage >&2; die "unknown option: $1" ;;
  esac
done

[ -n "$CORPUS" ] || { usage >&2; die "--corpus is required"; }
CAS="$CORPUS/lmdb"
IDS="$CORPUS/casid.txt"
[ -d "$CAS" ] || die "no corpus store at $CAS"
[ -f "$IDS" ] || die "no root ID file at $IDS"

mkdir -p "$BUILD"
BUILD=$(cd "$BUILD" && pwd)
BENCH="$BUILD/bin/llvm-cas-bench"
RESULTS=${RESULTS:-$BUILD/bench-results/$(date +%Y%m%d-%H%M%S)}
mkdir -p "$RESULTS/json" "$RESULTS/logs"

#===----------------------------------------------------------------------===#
# Build.
#===----------------------------------------------------------------------===#

CMAKE_ARGS=(
  -G Ninja -S "$SRC/llvm" -B "$BUILD"
  -DCMAKE_BUILD_TYPE=Release
  -DLLVM_ENABLE_ASSERTIONS=OFF
  -DLLVM_ENABLE_ONDISK_CAS=ON
  -DLLVM_TARGETS_TO_BUILD=host
  -DLLVM_INCLUDE_BENCHMARKS=OFF
  -DLLVM_INCLUDE_EXAMPLES=OFF
)
# Which SQLite is linked decides whether it can scale; see README.md. Prefer
# Homebrew's on macOS: the system one maps less of a database.
if command -v brew >/dev/null; then
  if P=$(brew --prefix sqlite 2>/dev/null) && [ -f "$P/include/sqlite3.h" ]; then
    CMAKE_ARGS+=(-DSQLite3_LIBRARY="$P/lib/libsqlite3.dylib"
                 -DSQLite3_INCLUDE_DIR="$P/include")
  fi
  if P=$(brew --prefix lmdb 2>/dev/null) && [ -d "$P" ]; then
    CMAKE_ARGS+=(-DLMDB_ROOT="$P")
  fi
fi
echo "Building llvm-cas-bench in $BUILD"
cmake "${CMAKE_ARGS[@]}" ${CMAKE_EXTRA[@]+"${CMAKE_EXTRA[@]}"} \
  > "$RESULTS/logs/cmake.log" 2>&1 ||
  die "cmake failed, see $RESULTS/logs/cmake.log"
ninja -C "$BUILD" llvm-cas-bench > "$RESULTS/logs/build.log" 2>&1 ||
  die "build failed, see $RESULTS/logs/build.log"

#===----------------------------------------------------------------------===#
# Record what was measured: results from two machines, or two sessions on
# one, are not comparable without it.
#===----------------------------------------------------------------------===#

cache_value() { sed -n "s/^$1:[A-Z]*=//p" "$BUILD/CMakeCache.txt"; }

{
  echo "date:      $(date '+%Y-%m-%d %H:%M:%S %z')"
  echo "source:    $(git -C "$SRC" log -1 --format='%h %s' 2>/dev/null || echo unknown)"
  if [ "$(uname)" = Darwin ]; then
    echo "machine:   $(sysctl -n hw.model), $(sysctl -n machdep.cpu.brand_string)"
    echo "memory:    $(($(sysctl -n hw.memsize) / 1073741824)) GiB"
    echo "os:        macOS $(sw_vers -productVersion) ($(sw_vers -buildVersion))"
  else
    echo "machine:   $(uname -m)"
    echo "os:        $(uname -sr)"
  fi
  echo "cores:     $NCPU"
  echo "build:     $(cache_value CMAKE_BUILD_TYPE), assertions $(cache_value LLVM_ENABLE_ASSERTIONS)"
  echo "compiler:  $("$(cache_value CMAKE_CXX_COMPILER)" --version | head -1)"
  echo "sqlite:    $(cache_value SQLite3_LIBRARY)"
  echo "lmdb:      $(cache_value LMDB_LIBRARY)"
} > "$RESULTS/environment.txt"
cat "$RESULTS/environment.txt"

#===----------------------------------------------------------------------===#
# Scratch space. On macOS, an APFS volume in the boot container: filesystem
# semantics and performance match the main volume, but deleting it is O(1),
# where unlinking the file backend's millions of files takes longer than
# creating them.
#===----------------------------------------------------------------------===#

VOLUME=casbench
VOLUME_DEV=""
MARKER=.llvm-cas-bench-scratch

plist_value() { plutil -extract "$1" raw -o - - 2>/dev/null; }

delete_volume() {
  [ -n "$VOLUME_DEV" ] || return 0
  echo "Deleting volume $VOLUME_DEV"
  diskutil unmount force "$VOLUME_DEV" >/dev/null 2>&1 || true
  diskutil apfs deleteVolume "$VOLUME_DEV" >/dev/null 2>&1 || true
}

if [ -z "$SCRATCH" ]; then
  [ "$(uname)" = Darwin ] || die "--scratch is required off macOS"
  if Old=$(diskutil info -plist "$VOLUME" 2>/dev/null | plist_value MountPoint) &&
     [ -n "$Old" ]; then
    # Never delete a volume this script did not make.
    [ -f "$Old/$MARKER" ] ||
      die "a volume named '$VOLUME' at $Old was not made by this script"
    VOLUME_DEV=$(diskutil info -plist "$Old" | plist_value DeviceIdentifier)
    delete_volume
  fi
  Container=$(diskutil info -plist / | plist_value APFSContainerReference)
  # Without -mountpoint this needs no root.
  Out=$(diskutil apfs addVolume "$Container" APFS "$VOLUME")
  VOLUME_DEV=$(sed -n 's/^Disk from APFS operation: //p' <<<"$Out")
  [ -n "$VOLUME_DEV" ] || die "could not create a volume: $Out"
  trap delete_volume EXIT
  SCRATCH=$(diskutil info -plist "$VOLUME_DEV" | plist_value MountPoint)
  for _ in $(seq 1 10); do
    [ -w "$SCRATCH" ] && break
    sleep 1
  done
  [ -w "$SCRATCH" ] || die "$SCRATCH did not become writable"
  touch "$SCRATCH/$MARKER" "$SCRATCH/.metadata_never_index"
  # Spotlight would otherwise index the file backend's files as they are
  # timed.
  mdutil -i off "$SCRATCH" >/dev/null 2>&1 || true
fi
mkdir -p "$SCRATCH"
echo "scratch:   $SCRATCH" | tee -a "$RESULTS/environment.txt"
echo

#===----------------------------------------------------------------------===#
# The runs.
#===----------------------------------------------------------------------===#

load_average() {
  if [ -r /proc/loadavg ]; then
    cut -d' ' -f1-3 /proc/loadavg
  else
    sysctl -n vm.loadavg | tr -d '{}' | sed 's/^ *//; s/ *$//'
  fi
}

run() { # run NAME ARGS...: one llvm-cas-bench process
  local Name=$1
  shift
  # The load going in says whether something else was competing for the
  # machine. A parallel run just before inflates it for a minute or so.
  printf '%s  %-24s load %s\n' "$(date +%H:%M:%S)" "$Name" "$(load_average)"
  "$BENCH" "$@" --json "$RESULTS/json/$Name.json" \
    > "$RESULTS/logs/$Name.log" 2>&1 ||
    die "$Name failed, see $RESULTS/logs/$Name.log"
}

round() { # round BACKEND RUN
  local B=$1 R=$2 Dir="$SCRATCH/$1-r$2" T
  local Store=(--ids "$IDS" --backend="$B" --out "$Dir")

  rm -rf "$Dir"
  run "insert-$B-r$R" --cas "$CAS" --source-backend=lmdb "${Store[@]}" --insert
  run "traverse-$B-t1-r$R" "${Store[@]}" --traverse
  for T in $((NCPU / 2)) "$NCPU"; do
    run "traverse-$B-t$T-r$R" "${Store[@]}" --traverse --parallel-traverse \
      --traverse-threads="$T"
  done
  # The file backend's store is left for the volume to take with it.
  [ "$B" = file ] || rm -rf "$Dir"
}

# Round by round, so a drift in the machine's state lands on both backends
# instead of on whichever ran last.
for R in $(seq 1 "$RUNS"); do
  round llvmcas "$R"
  round lmdb "$R"
done
round sqlite 1
round file 1

python3 "$SCRIPT_DIR/summarize-bench.py" "$RESULTS" > "$RESULTS/summary.md"
echo
cat "$RESULTS/summary.md"
echo
echo "Results in $RESULTS"
