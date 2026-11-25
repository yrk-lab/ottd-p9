#!/bin/bash
# Compile the needed non-header parts of libc++ to LLVM IR, link them into one module.
set -euo pipefail; . "$(dirname "$0")/common.sh"
SRC=$W/llvm-src/libcxx/src; D=$OUT/libcxx; mkdir -p "$D"
for f in $LIBCXX_SRCS; do
  clang++-17 $CXXFLAGS -S -emit-llvm -D_LIBCPP_BUILDING_LIBRARY -DLIBCXX_BUILDING_LIBRARY \
    -I"$SRC" -I"$SRC/include" "$SRC/$f.cpp" -o "$D/$(basename $f).ll"
done
llvm-link "$D"/*.ll -S -o "$OUT/libcxx_min.ll"
echo "libcxx_min.ll: $(wc -c < "$OUT/libcxx_min.ll") bytes"
