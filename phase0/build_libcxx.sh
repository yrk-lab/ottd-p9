#!/bin/bash
# build_libcxx [eh]
#	Compile the parts of libc++ that are not header-only to LLVM IR
#	and link them into one module, out/libcxx_min.ll.
#	eh: the same with exceptions and RTTI, plus the std exception
#	classes and typeinfo from libc++abi and eh/eh_match.cpp, as
#	out/eh/libcxx_eh.ll (for run_eh.sh and p9check -E).
set -euo pipefail
. "$(dirname "$0")/common.sh"

src=$work/llvm-src/libcxx/src
abi=$work/llvm-src/libcxxabi

plain()
{
	local d=$out/libcxx f
	mkdir -p "$d"
	for f in $libcxxsrcs; do
		clang++-17 $cxxflags -S -emit-llvm -D_LIBCPP_BUILDING_LIBRARY -DLIBCXX_BUILDING_LIBRARY \
			-I"$src" -I"$src/include" "$src/$f.cpp" -o "$d/$(basename "$f").ll"
	done
	llvm-link "$d"/*.ll -S -o "$out/libcxx_min.ll"
	echo "libcxx_min.ll: $(wc -c <"$out/libcxx_min.ll") bytes"
}

eh()
{
	local d=$out/eh/lib f
	local flags="--target=$triple -std=c++20 -stdlib=libc++ -O1 -g0 -D_LIBCPP_DISABLE_EXTERN_TEMPLATE"
	mkdir -p "$d"
	# as when libc++ is built on libc++abi: the std exception classes come from there
	for f in $libcxxsrcs stdexcept; do
		clang++-17 $flags -S -emit-llvm -D_LIBCPP_BUILDING_LIBRARY -DLIBCXX_BUILDING_LIBRARY \
			-DLIBCXX_BUILDING_LIBCXXABI -D_LIBCPPABI_VERSION=17000 \
			-I"$src" -I"$src/include" "$src/$f.cpp" -o "$d/cxx_$(basename "$f").ll"
	done
	for f in private_typeinfo stdlib_typeinfo stdlib_exception stdlib_stdexcept; do
		clang++-17 $flags -S -emit-llvm -D_LIBCXXABI_BUILDING_LIBRARY -D_LIBCPP_BUILDING_LIBRARY \
			-I"$abi/include" -I"$abi/src" -I"$src" -I"$src/include" \
			"$abi/src/$f.cpp" -o "$d/abi_$f.ll"
	done
	clang++-17 $flags -S -emit-llvm -I"$abi/include" -I"$abi/src" "$top/eh/eh_match.cpp" -o "$d/eh_match.ll"
	llvm-link "$d"/*.ll -S -o "$out/eh/libcxx_eh.ll"
	echo "libcxx_eh.ll: $(wc -c <"$out/eh/libcxx_eh.ll") bytes"
}

case ${1:-} in
'')	plain ;;
eh)	eh ;;
*)	fatal "usage: build_libcxx [eh]" ;;
esac
