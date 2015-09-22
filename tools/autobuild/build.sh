#!/bin/bash
set -e -x

LOCAL_DIRNAME="$(dirname "$0")"
#LOCAL_DIRNAME="${PWD}/$(dirname "$0")"

source "$LOCAL_DIRNAME/detect_qmake.sh"

# Prints number of cores to stdout
GetCPUCores() {
  case "$OSTYPE" in
    linux-gnu) grep -c ^processor /proc/cpuinfo 2>/dev/null
               ;;
    darwin*) sysctl -n hw.ncpu
               ;;
    *)         echo "Unsupported platform in $0"
               exit 1
               ;;
  esac
  return 0
}

# 1st param: shadow directory path
# 2nd param: mkspec
# 3rd param: additional qmake parameters
BuildQt() {
  (
    # set qmake path
    PATH="$(PrintQmakePath):$PATH" || ( echo "ERROR: qmake was not found, please add it to your PATH or into the tools/autobuild/detect_qmake.sh"; exit 1 )
    SHADOW_DIR="$1"
    MKSPEC="$2"
    QMAKE_PARAMS="$3"

    mkdir -p "$SHADOW_DIR"
    cd "$SHADOW_DIR"
    qmake -r "$QMAKE_PARAMS" -spec "$MKSPEC" "$LOCAL_DIRNAME/../../omim.pro"
#    make clean > /dev/null || true
    make -j $(GetCPUCores)
  )
}
