#!/bin/bash
# Phase 0 setup: toolchain, patched llvm-cbe, libc++ 17 sources. Ubuntu 24.04 / WSL2 (needs sudo for apt).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); W=${W:-$HERE/work}; mkdir -p "$W"
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO=sudo
$SUDO apt-get update -qq
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  clang-17 llvm-17 llvm-17-dev libclang-rt-17-dev libc++-17-dev libc++abi-17-dev libunwind-17-dev \
  cmake ninja-build git ca-certificates gcc python3 libzstd-dev zlib1g-dev
# llvm-cbe: master now needs LLVM 22; this is the last revision before LLVM 19 support (works with 17).
CBE_REV=5341f88
[ -d "$W/llvm-cbe" ] || git clone https://github.com/JuliaComputing/llvm-cbe "$W/llvm-cbe"
( cd "$W/llvm-cbe" && git checkout -q -f $CBE_REV && git apply "$HERE/patches/llvm-cbe-phase0.patch" )
# CMake >= 4 dropped compatibility with cmake_minimum_required < 3.5, which llvm-cbe's old CMakeLists still declares
# (3.4.3). The policy flag overrides that; the build dir is wiped so a cache from an earlier failed configure can't stick.
rm -rf "$W/llvm-cbe/build"
cmake -S "$W/llvm-cbe" -B "$W/llvm-cbe/build" -G Ninja -Wno-dev -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DLLVM_DIR=/usr/lib/llvm-17/lib/cmake/llvm -DCMAKE_C_COMPILER=clang-17 -DCMAKE_CXX_COMPILER=clang++-17 >/dev/null
ninja -C "$W/llvm-cbe/build" llvm-cbe
# libc++ 17.0.6 sources (non-header parts get translated too). Sparse clone: ~12 MB.
if [ ! -d "$W/llvm-src" ]; then
  git clone --depth 1 --branch llvmorg-17.0.6 --filter=blob:none --sparse https://github.com/llvm/llvm-project.git "$W/llvm-src"
  git -C "$W/llvm-src" sparse-checkout set libcxx/src libcxx/include
fi
echo "setup ok: $W/llvm-cbe/build/tools/llvm-cbe/llvm-cbe"
