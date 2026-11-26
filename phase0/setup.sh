#!/bin/bash
# setup
#	Phase 0 setup: clang/LLVM 17 and libc++, the pinned and patched
#	llvm-cbe, and the libc++ 17.0.6 sources.  Needs sudo for apt.
set -euo pipefail
. "$(dirname "$0")/common.sh"

# llvm-cbe master needs LLVM 22; this is the last revision before
# LLVM 19 support, and works with 17.
rev=5341f88

mkdir -p "$work"
sudo apt-get update -qq
sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
	clang-17 llvm-17 llvm-17-dev libclang-rt-17-dev libc++-17-dev libc++abi-17-dev libunwind-17-dev \
	cmake ninja-build git ca-certificates gcc python3 libzstd-dev zlib1g-dev

[ -d "$work/llvm-cbe" ] || git clone https://github.com/JuliaComputing/llvm-cbe "$work/llvm-cbe"
git -C "$work/llvm-cbe" checkout -q -f $rev
git -C "$work/llvm-cbe" apply "$top/patches/llvm-cbe-phase0.patch"

# CMake 4 rejects llvm-cbe's cmake_minimum_required(3.4.3); the
# policy flag overrides that.  The build directory is wiped so that
# a cache from an earlier failed configure cannot stick.
rm -rf "$work/llvm-cbe/build"
cmake -S "$work/llvm-cbe" -B "$work/llvm-cbe/build" -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DLLVM_DIR=/usr/lib/llvm-17/lib/cmake/llvm \
	-DCMAKE_C_COMPILER=clang-17 -DCMAKE_CXX_COMPILER=clang++-17 >/dev/null
ninja -C "$work/llvm-cbe/build" llvm-cbe

# libc++ sources: the non-header parts are translated too.
# Sparse clone, about 12 MB.
if [ ! -d "$work/llvm-src" ]; then
	git clone --depth 1 --branch llvmorg-17.0.6 --filter=blob:none --sparse \
		https://github.com/llvm/llvm-project.git "$work/llvm-src"
	git -C "$work/llvm-src" sparse-checkout set libcxx/src libcxx/include
fi

echo "setup ok: $cbe"
