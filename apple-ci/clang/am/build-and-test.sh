#!/usr/bin/env bash
set -eu

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
source ${SCRIPT_DIR}/build.sh

$NINJA check-clang check-llvm
