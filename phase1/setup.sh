#!/bin/bash
# setup
#	Fetch the OpenTTD release, zlib and xz (translated with it), the
#	OpenGFX, OpenSFX and OpenMSX base sets, and make the stock source
#	tree src-ref.  Both src-ref and the patched
#	src-p9 are exported without .git and share one .ottdrev, so the
#	two builds carry the same version string and their saves can be
#	compared byte for byte.
set -euo pipefail
. "$(dirname "$0")/common.sh"

mkdir -p "$work"
sudo apt-get install -y --no-install-recommends \
	cmake ninja-build gcc g++ zlib1g-dev liblzma-dev curl unzip >/dev/null

if [ ! -d "$git" ]; then
	git -c advice.detachedHead=false clone -q --depth 1 --branch "$tag" https://github.com/OpenTTD/OpenTTD.git "$git"
fi

# version line as FindVersion.cmake would write it for the clean tag
hash=$(git -C "$git" rev-parse HEAD)
date=$(git -C "$git" log -1 --format=%cd --date=format:%Y%m%d)
printf '%s\t%s\t0\t%s\t1\t1\n' "$tag" "$date" "$hash" >"$work/ottdrev"

rm -rf "$ref"
mkdir -p "$ref"
git -C "$git" archive HEAD | tar -x -C "$ref"
cp "$work/ottdrev" "$ref/.ottdrev"

if [ ! -d "$zlib" ]; then
	git -c advice.detachedHead=false clone -q --depth 1 --branch "$zlibtag" https://github.com/madler/zlib.git "$zlib"
fi

if [ ! -d "$xz" ]; then
	git -c advice.detachedHead=false clone -q --depth 1 --branch "$xztag" https://github.com/tukaani-project/xz.git "$xz"
fi

# the free sound and music sets, as OpenGFX: name-version-all.zip
# holding name-version.tar
for set in opensfx:$opensfx openmsx:$openmsx; do
	n=${set%%:*} v=${set#*:}
	[ -d "$data/openttd/baseset/$n" ] && continue
	mkdir -p "$data/openttd/baseset"
	curl -fsSL -o "$work/$n.zip" "https://cdn.openttd.org/$n-releases/$v/$n-$v-all.zip"
	unzip -q -o "$work/$n.zip" -d "$work/$n"
	tar -x -C "$data/openttd/baseset" -f "$work/$n/$n-$v.tar"
	mv "$data/openttd/baseset/$n-$v" "$data/openttd/baseset/$n"
done

# the instrument bank for 9front's MIDI player (games/dmid): Freedoom's
# GENMIDI lump (BSD licence), from its WAD
if [ ! -f "$data/genmidi" ]; then
	curl -fsSL -o "$work/freedoom.zip" \
		"https://github.com/freedoom/freedoom/releases/download/v$freedoom/freedoom-$freedoom.zip"
	unzip -q -o -j "$work/freedoom.zip" "freedoom-$freedoom/freedoom2.wad" -d "$work/freedoom"
	python3 - "$work/freedoom/freedoom2.wad" "$data/genmidi" <<'PY'
import struct, sys
w = open(sys.argv[1], "rb").read()
n, off = struct.unpack("<ii", w[4:12])
for i in range(n):
	o, s, name = struct.unpack("<ii8s", w[off + 16*i:off + 16*i + 16])
	if name.rstrip(b"\0") == b"GENMIDI":
		open(sys.argv[2], "wb").write(w[o:o + s])
		break
else:
	sys.exit("no GENMIDI in " + sys.argv[1])
PY
fi

if [ ! -d "$data/openttd/baseset/opengfx" ]; then
	mkdir -p "$data/openttd/baseset"
	curl -fsSL -o "$work/opengfx.zip" "https://cdn.openttd.org/opengfx-releases/$opengfx/opengfx-$opengfx-all.zip"
	unzip -q -o "$work/opengfx.zip" -d "$work/opengfx"
	tar -x -C "$data/openttd/baseset" -f "$work"/opengfx/opengfx-$opengfx.tar
	mv "$data/openttd/baseset/opengfx-$opengfx" "$data/openttd/baseset/opengfx"
fi

echo "setup ok: $ref, $data/openttd/baseset/opengfx"
