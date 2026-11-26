#!/bin/bash
# build_p9 [libcxx|noexc]
#	Build OpenTTD the way the translator will see it: clang 17 (the
#	LLVM llvm-cbe is pinned to) with libc++, dedicated server only,
#	from src-p9 (src-ref with patches/series applied; prep.sh).
#	libcxx	exceptions and RTTI still on: checks that the compiler
#		and library change alone keep the game identical
#	noexc	-fno-exceptions -fno-rtti, the target (default)
set -euo pipefail
. "$(dirname "$0")/common.sh"

mode=${1:-noexc}
case $mode in
libcxx)	flags="-stdlib=libc++" ;;
noexc)	flags="-stdlib=libc++ -fno-exceptions -fno-rtti" ;;
*)	fatal "usage: build_p9 [libcxx|noexc]" ;;
esac

b=$work/build-$mode
log=$work/build-$mode.log
[ -d "$p9" ] || fatal "no $p9: run prep.sh"
cmake -S "$p9" -B "$b" -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER=clang-17 \
	-DCMAKE_CXX_COMPILER=clang++-17 \
	-DCMAKE_CXX_FLAGS="$flags" \
	-DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++" \
	-DOPTION_DEDICATED=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_LZO=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_PNG=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_CURL=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_unofficial-breakpad=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Grfcodec=ON >"$log"
if ! ninja -C "$b" -k 0 openttd >>"$log" 2>&1; then
	grep -c 'error:' "$log" | sed 's/^/errors: /'
	fatal "build failed; log in $log"
fi
echo "built $b/openttd"
