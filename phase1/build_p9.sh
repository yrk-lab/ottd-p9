#!/bin/bash
# build_p9
#	Build OpenTTD the way the translator will see it: clang 17 (the
#	LLVM llvm-cbe is pinned to) with libc++, exceptions and RTTI on,
#	dedicated server only, from src-p9 (src-ref with patches/series
#	applied; prep.sh), into work/build-p9.
#	No threads: nothreads.c makes pthread_create fail, so OpenTTD's
#	StartNewThread falls back to running the work inline, as it will
#	on Plan 9.
#	No SSE (Plan 9 compilers have no vector types): the blitters are
#	not part of the simulation.  compile_commands.json is exported for
#	translate.sh, which recompiles every unit to LLVM IR.
#	The build-time tools strgen and settingsgen run on Linux and are
#	never translated: they are taken from the reference build
#	(HOST_BINARY_DIR, upstream's cross-compiling mechanism).
set -euo pipefail
. "$(dirname "$0")/common.sh"

b=$work/build-p9
log=$work/build-p9.log
[ -d "$p9" ] || fatal "no $p9: run prep.sh"
[ -f "$work/build-ref/strgen.cmake" ] || fatal "no host tools: run build_ref.sh"
clang-17 -O1 -c "$top/nothreads.c" -o "$work/nothreads.o"
cmake -S "$p9" -B "$b" -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER=clang-17 \
	-DCMAKE_CXX_COMPILER=clang++-17 \
	-DCMAKE_CXX_FLAGS="-stdlib=libc++" \
	-DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++ $work/nothreads.o" \
	-DOPTION_DEDICATED=ON \
	-DHOST_BINARY_DIR="$work/build-ref" \
	-DCMAKE_DISABLE_FIND_PACKAGE_LZO=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_PNG=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_CURL=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_unofficial-breakpad=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Grfcodec=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_SSE=ON -DSSE_FOUND=OFF \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON >"$log"
if ! ninja -C "$b" -k 0 openttd >>"$log" 2>&1; then
	grep -c 'error:' "$log" | sed 's/^/errors: /'
	fatal "build failed; log in $log"
fi
echo "built $b/openttd"
