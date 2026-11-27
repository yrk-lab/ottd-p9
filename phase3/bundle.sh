#!/bin/bash
# bundle
#	OpenTTD for 9front as one file, named for the game's version and
#	the system: work/openttd-<version>-plan9-amd64.tgz (the version is
#	the release's, ../phase1/work/ottdrev), holding the directory
#	openttd/ with the binary as Plan 9 names it before it
#	is installed (6.openttd), the data the game finds next to it (lang,
#	ai, game, media, bin, baseset with OpenGFX, OpenSFX and OpenMSX),
#	the licences, and README (bundle.README: how to run and install
#	it).  The binary is the one ../phase2/run9front.sh built on 9front
#	(work/tu/obj.9front/6.openttd): run that first.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../phase1/common.sh"
bin=$here/../phase2/work/tu/obj.9front/6.openttd
[ -f "$bin" ] || fatal "no $bin: run ../phase2/run9front.sh"
for s in opengfx opensfx openmsx; do
	[ -d "$data/openttd/baseset/$s" ] || fatal "no $s: run ../phase1/setup.sh"
done

version=$(cut -f1 "$work/ottdrev")
[ -n "$version" ] || fatal "no version in $work/ottdrev: run ../phase1/setup.sh"
tgz=$here/work/openttd-$version-plan9-amd64.tgz
b=$here/work/bundle
rm -rf "${b:?}"
mkdir -p "$b/openttd"
cp "$bin" "$b/openttd/6.openttd"
# the build's data (-L: copies, not links)
for d in lang ai game media bin baseset; do
	cp -rL "$work/build-p9/$d" "$b/openttd/$d"
done
for s in opengfx opensfx openmsx; do
	cp -rL "$data/openttd/baseset/$s" "$b/openttd/baseset/$s"
done
mkdir -p "$b/openttd/doc"
cp "$ref/COPYING.md" "$b/openttd/doc/COPYING.md"
cp "$here/bundle.README" "$b/openttd/README"
# Plan 9's tar reads ustar; owner and times as in a release
tar -C "$b" --format=ustar --owner=0 --group=0 --numeric-owner -czf "$tgz" openttd
echo "$tgz: $(du -h "$tgz" | cut -f1)"
