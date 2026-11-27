#!/bin/bash
# buildtu
#	Phase 2.5: the translated OpenTTD built unit by unit
#	(../phase0/tu/build.sh) into work/tu, from the units of
#	../phase1/translate.sh's ir step (work/tr/ir/units.txt, link
#	order).  A unit is named after its path, flat: script/api/
#	script_map.cpp is script_api_script_map.  Only what changed is
#	translated and compiled again; the goken build is
#	work/tu/obj.goken/6.openttd (sys="goken linux" adds the Linux one,
#	obj.linux/o.openttd, ../phase3/run_p9p.sh's), and 9front builds the same
#	tree with mk SYS=9front (run9front.sh).  The data directories are linked
#	next to it, as for the Linux builds.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
out= work=
. "$here/../phase0/common.sh"
p1=$(realpath "$here/../phase1/work")
ir=$p1/tr/ir
d=$here/work/tu
[ -f "$ir/units.txt" ] || fatal "no $ir/units.txt: run ../phase1/translate.sh ir"
mkdir -p "$d"

sed "s|^$ir/||" "$ir/units.txt" | while read -r p; do
	n=$(echo "${p%.bc}" | sed 's/\.cpp$//; s/\.c$//; s|/|_|g; s/[^A-Za-z0-9_]/_/g')
	echo "$n $ir/$p"
done >"$d/program.new"
cmp -s "$d/program.new" "$d/program" || mv "$d/program.new" "$d/program"
rm -f "$d/program.new"

"$top/tu/build.sh" "$d/program" "$d" openttd || exit 1
for o in "$d"/obj.goken "$d"/obj.linux; do
	[ -d "$o" ] || continue
	for x in lang ai game baseset media bin; do
		ln -sfn "$p1/build-p9/$x" "$o/$x"
	done
done
