#!/bin/bash
# unit in.bc in.home name dir
#	Translate one separately compiled unit to Plan 9 C: settle its
#	shared definitions (localize.py, with homes.py's in.home), lower
#	its exceptions (lowereh), llvm-cbe, lower.py --libc --unit.  Writes
#	dir/name.c, its exception wrappers dir/name.inv and its
#	constructors dir/name.ctors, each only if it changed, so that mk
#	rebuilds nothing more.  The steps between stay in memory
#	(/dev/shm), not on disk.
set -uo pipefail
. "$(dirname "$0")/../common.sh"
[ $# -eq 4 ] || fatal "usage: unit in.bc in.home name dir"
bc=$1 home=$2 name=$3 dir=$4
tu=$top/tu
t=$(mktemp -d /dev/shm/tu.XXXXXX) || fatal "no /dev/shm"
trap 'rm -rf "$t"' EXIT

# move new onto old unless they are the same
update()
{
	cmp -s "$1" "$2" || mv "$1" "$2"
}

llvm-dis-17 -o - "$bc" | python3 "$tu/localize.py" "$home" |
	opt-17 -passes=elim-avail-extern -S -o "$t/loc.ll" &&
lowereh "$t/loc.ll" "$t/eh.ll" "$t/inv" &&
"$cbe" -cbe-explicit-padding "$t/eh.ll" -o "$t/cbe.c" &&
python3 "$top/plan9/lower.py" --libc "--unit=$name,$t/ctors" <"$t/cbe.c" >"$t/c" ||
	fatal "$bc: translation failed"
update "$t/c" "$dir/$name.c"
update "$t/inv" "$dir/$name.inv"
update "$t/ctors" "$dir/$name.ctors"
