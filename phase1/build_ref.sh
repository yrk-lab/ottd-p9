#!/bin/bash
# build_ref
#	The reference: stock OpenTTD (src-ref) as upstream builds it on
#	Linux (system compiler, libstdc++, exceptions, threads), the game
#	with its GUI (no video, sound or music library: the GUI draws to
#	the null driver, which plays the gate; -D runs a dedicated
#	server).  Libraries the Plan 9 build will not have are left out
#	(they do not affect the simulation): only zlib and liblzma (the
#	system's: the translated build has the same versions), no fonts
#	library (the built-in sprite font).
#	ALLEGRO_FOUND only satisfies upstream's "SDL2 or Allegro" check: the
#	Allegro driver's files test Allegro_FOUND, which stays unset.
set -euo pipefail
. "$(dirname "$0")/common.sh"

b=$work/build-ref
[ -d "$ref" ] || fatal "no $ref: run setup.sh"
cmake -S "$ref" -B "$b" -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DOPTION_DEDICATED=OFF \
	-DALLEGRO_FOUND=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Fontconfig=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Harfbuzz=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_ICU=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_SDL2=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Allegro=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_OpenGL=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Fluidsynth=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_OpusFile=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_LZO=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_LibLZMA=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_PNG=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_CURL=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_unofficial-breakpad=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Grfcodec=ON >"$work/build-ref.log"
ninja -C "$b" openttd >>"$work/build-ref.log" 2>&1 || {
	tail -20 "$work/build-ref.log"
	fatal "build failed; log in $work/build-ref.log"
}
echo "built $b/openttd"
