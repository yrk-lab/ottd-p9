#!/bin/bash
# test [tN...]
#	The separately translated tests (../tests_tu/tN: C++ units and
#	the output a native build gives, expected): each unit compiled to
#	IR, built unit by unit (build.sh) into out/tu/tN for goken (no
#	threads) and Linux with plan9port (the program's threads on
#	glibc's: thread_p9.c, threadp9p.c), run and compared.  The tree is left for 9front, which builds it with
#	mk SYS=9front: cd /n/ottd/phase0/out/tu/tN; mk SYS=9front;
#	obj.9front/6.tN (../../phase2/vmsh).
set -uo pipefail
. "$(dirname "$0")/../common.sh"
[ $# -gt 0 ] || set -- $(cd "$top/tests_tu" && ls)
fail=0
for t in "$@"; do
	src=$top/tests_tu/$t
	d=$out/tu/$t
	mkdir -p "$d/ir"
	: >"$d/units.new"
	for f in "$src"/*.cpp; do
		n=$(basename "${f%.cpp}")
		clang++-17 $cxxflags -c -emit-llvm "$f" -o "$d/ir/$n.bc.new" || { fail=1; continue 2; }
		cmp -s "$d/ir/$n.bc.new" "$d/ir/$n.bc" || mv "$d/ir/$n.bc.new" "$d/ir/$n.bc"
		rm -f "$d/ir/$n.bc.new"
		echo "$n $d/ir/$n.bc" >>"$d/units.new"
	done
	cmp -s "$d/units.new" "$d/units.list" || mv "$d/units.new" "$d/units.list"
	rm -f "$d/units.new"
	sys="goken linux" "$top/tu/build.sh" "$d/units.list" "$d" "$t" >"$d/build.log" 2>&1 || {
		echo "$t: BUILD FAILED"; tail -5 "$d/build.log"; fail=1; continue
	}
	for b in obj.goken/6 obj.linux/o; do
		timeout 120 "$d/$b.$t" >"$d/out" 2>&1
		if cmp -s "$d/out" "$src/expected"; then
			echo "$t ($b.$t): PASS"
		else
			echo "$t ($b.$t): DIFFERENT"
			diff "$src/expected" "$d/out" | sed 10q
			fail=1
		fi
	done
done
exit $fail
