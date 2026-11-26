#!/bin/bash
# build_libcxx
#	Compile the parts of libc++ that are not header-only to LLVM IR
#	and link them into one module, out/libcxx_min.ll.
set -euo pipefail
. "$(dirname "$0")/common.sh"

src=$work/llvm-src/libcxx/src
d=$out/libcxx
mkdir -p "$d"
for f in $libcxxsrcs; do
	clang++-17 $cxxflags -S -emit-llvm -D_LIBCPP_BUILDING_LIBRARY -DLIBCXX_BUILDING_LIBRARY \
		-I"$src" -I"$src/include" "$src/$f.cpp" -o "$d/$(basename "$f").ll"
done
llvm-link "$d"/*.ll -S -o "$out/libcxx_min.ll"
echo "libcxx_min.ll: $(wc -c <"$out/libcxx_min.ll") bytes"
