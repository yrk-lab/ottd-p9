#!/bin/bash
# build_ref
#	The reference: stock OpenTTD (src-ref) as upstream builds it on
#	Linux (system compiler, libstdc++, exceptions, threads), dedicated
#	server only.  Libraries the Plan 9 build will not have are left
#	out (they do not affect the simulation): only zlib and liblzma.
set -euo pipefail
. "$(dirname "$0")/common.sh"

b=$work/build-ref
[ -d "$ref" ] || fatal "no $ref: run setup.sh"
cmake -S "$ref" -B "$b" -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DOPTION_DEDICATED=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_LZO=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_PNG=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_CURL=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_unofficial-breakpad=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Grfcodec=ON >"$work/build-ref.log"
ninja -C "$b" openttd >>"$work/build-ref.log" 2>&1 || {
	tail -20 "$work/build-ref.log"
	fatal "build failed; log in $work/build-ref.log"
}
echo "built $b/openttd"
