#!/bin/bash
# build_p9
#	Build OpenTTD the way the translator will see it: clang 17 (the
#	LLVM llvm-cbe is pinned to) with libc++, exceptions and RTTI on,
#	dedicated server only, from src-p9 (src-ref with patches/series
#	applied; prep.sh), into work/build-p9.
#	The build-time tools strgen and settingsgen run on Linux and are
#	never translated: they are taken from the reference build
#	(HOST_BINARY_DIR, upstream's cross-compiling mechanism).
set -euo pipefail
. "$(dirname "$0")/common.sh"

b=$work/build-p9
log=$work/build-p9.log
[ -d "$p9" ] || fatal "no $p9: run prep.sh"
[ -f "$work/build-ref/strgen.cmake" ] || fatal "no host tools: run build_ref.sh"
cmake -S "$p9" -B "$b" -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER=clang-17 \
	-DCMAKE_CXX_COMPILER=clang++-17 \
	-DCMAKE_CXX_FLAGS="-stdlib=libc++" \
	-DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++" \
	-DOPTION_DEDICATED=ON \
	-DHOST_BINARY_DIR="$work/build-ref" \
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
