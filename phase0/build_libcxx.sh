#!/bin/bash
# build_libcxx
#	Compile the parts of libc++ that are not header-only, the parts of
#	libc++abi the translated programs need (std exception classes,
#	typeinfo and dynamic_cast) and eh/eh_match.cpp to LLVM IR, with
#	exceptions and RTTI, and link them into one module, out/libcxx.ll.
set -euo pipefail
. "$(dirname "$0")/common.sh"

src=$work/llvm-src/libcxx/src
abi=$work/llvm-src/libcxxabi
d=$out/libcxx-ir
mkdir -p "$d"

# as when libc++ is built on libc++abi: the std exception classes come from there
for f in $libcxxsrcs stdexcept; do
	clang++-17 $cxxflags -S -emit-llvm -D_LIBCPP_BUILDING_LIBRARY -DLIBCXX_BUILDING_LIBRARY \
		-DLIBCXX_BUILDING_LIBCXXABI -D_LIBCPPABI_VERSION=17000 \
		-I"$src" -I"$src/include" "$src/$f.cpp" -o "$d/cxx_$(basename "$f").ll"
done
for f in private_typeinfo stdlib_typeinfo stdlib_exception stdlib_stdexcept; do
	clang++-17 $cxxflags -S -emit-llvm -D_LIBCXXABI_BUILDING_LIBRARY -D_LIBCPP_BUILDING_LIBRARY \
		-I"$abi/include" -I"$abi/src" -I"$src" -I"$src/include" \
		"$abi/src/$f.cpp" -o "$d/abi_$f.ll"
done
clang++-17 $cxxflags -S -emit-llvm -I"$abi/include" -I"$abi/src" "$top/eh/eh_match.cpp" -o "$d/eh_match.ll"
llvm-link "$d"/*.ll -S -o "$out/libcxx.ll"
echo "libcxx.ll: $(wc -c <"$out/libcxx.ll") bytes"
