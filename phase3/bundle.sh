#!/bin/bash
# bundle
#	OpenTTD for 9front (or, with sys=9legacy, 9legacy) as one file,
#	named for the game's version and the system:
#	work/openttd-<version>-9front-amd64.tgz, or -plan9-amd64 for 9legacy
#	(Plan 9 from Bell Labs carried on; the version is
#	the release's, ../phase1/work/ottdrev), holding the directory
#	openttd/ with the binary as Plan 9 names it before it
#	is installed (6.openttd), the data the game finds next to it (lang,
#	ai, game, media, bin, baseset with OpenGFX, OpenSFX and OpenMSX),
#	the licences (OpenTTD's, and FreeType's: it is linked in), OpenTTD's
#	README.md, README.Plan9 (bundle.README.Plan9, the system named:
#	how to run and install it) and an mkfile that installs it
#	(bundle.mkfile: mk install, BIN as usual).  The binary is the one
#	../phase2/run9front.sh built on 9front (work/tu/obj.9front), or
#	../phase5/run9legacy.sh on 9legacy (obj.9legacy): run that first.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../phase1/common.sh"
# sys=9legacy: the binary build.rc built (../phase5/run9legacy.sh)
sys=${sys:-9front}
case $sys in
9front)	name=9front; title=9front; run=../phase2/run9front.sh ;;
9legacy)	name=plan9; title=9legacy; run="../phase5/run9legacy.sh (script=phase5/build.rc)" ;;
*)	fatal "sys=$sys: 9front or 9legacy" ;;
esac
bin=$here/../phase2/work/tu/obj.$sys/6.openttd
[ -f "$bin" ] || fatal "no $bin: run $run"
for s in opengfx opensfx openmsx; do
	[ -d "$data/openttd/baseset/$s" ] || fatal "no $s: run ../phase1/setup.sh"
done

version=$(cut -f1 "$work/ottdrev")
[ -n "$version" ] || fatal "no version in $work/ottdrev: run ../phase1/setup.sh"
tgz=$here/work/openttd-$version-$name-amd64.tgz
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
cp "$freetype/docs/FTL.TXT" "$b/openttd/doc/FreeType-FTL.TXT"
sed "s/@SYSTEM@/$title/g" "$here/bundle.README.Plan9" >"$b/openttd/README.Plan9"
cp "$here/bundle.mkfile" "$b/openttd/mkfile"
cp "$ref/README.md" "$b/openttd/README.md"
# Plan 9's tar reads ustar; owner and times as in a release
tar -C "$b" --format=ustar --owner=0 --group=0 --numeric-owner -czf "$tgz" openttd
echo "$tgz: $(du -h "$tgz" | cut -f1)"
