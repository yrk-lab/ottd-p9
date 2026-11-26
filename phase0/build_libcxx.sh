#!/bin/bash
# build_libcxx
#	Compile the parts of libc++ that are not header-only, the parts of
#	libc++abi the translated programs need (std exception classes,
#	typeinfo and dynamic_cast) and eh/eh_match.cpp to LLVM IR, with
#	exceptions and RTTI: one bitcode module per source in out/libcxx-ir/,
#	an archive for linkcxx (arlink.py) to take what a program needs from.
set -euo pipefail
. "$(dirname "$0")/common.sh"

src=$work/llvm-src/libcxx/src
abi=$work/llvm-src/libcxxabi
d=$out/libcxx-ir
mkdir -p "$d"

# as when libc++ is built on libc++abi: the std exception classes come from there
for f in $libcxxsrcs stdexcept; do
	clang++-17 $cxxflags -emit-llvm -D_LIBCPP_BUILDING_LIBRARY -DLIBCXX_BUILDING_LIBRARY \
		-DLIBCXX_BUILDING_LIBCXXABI -D_LIBCPPABI_VERSION=17000 \
		-I"$src" -I"$src/include" "$src/$f.cpp" -c -o "$d/cxx_$(basename "$f").bc"
done
for f in private_typeinfo stdlib_typeinfo stdlib_exception stdlib_stdexcept; do
	clang++-17 $cxxflags -emit-llvm -D_LIBCXXABI_BUILDING_LIBRARY -D_LIBCPP_BUILDING_LIBRARY \
		-I"$abi/include" -I"$abi/src" -I"$src" -I"$src/include" \
		-c "$abi/src/$f.cpp" -o "$d/abi_$f.bc"
done
clang++-17 $cxxflags -emit-llvm -c -I"$abi/include" -I"$abi/src" "$top/eh/eh_match.cpp" -o "$d/eh_match.bc"
touch "$d/stamp"
echo "$d: $(ls "$d"/*.bc | wc -l) modules"
