#!/bin/bash
# run_p9
#	Phase 0.5 fast loop: every C++ test in tests/ and tests_p9/
#	through p9check (translator + goken 6c on Linux), and the
#	exception tests in tests_eh/ with p9check -E.
#	Run plan9/setup_goken.sh once first.
set -uo pipefail
. "$(dirname "$0")/../common.sh"

[ -f "$out/libcxx_min.ll" ] || "$top/build_libcxx.sh" || exit 1
[ -f "$out/eh/libcxx_eh.ll" ] || "$top/build_libcxx.sh" eh || exit 1

# one test: p9check flags... file
one()
{
	n=$((n+1))
	r=$("$top/plan9/p9check.sh" "$@")
	echo "$r"
	case $r in
	*": PASS")	pass=$((pass+1)) ;;
	esac
}

pass=0
n=0
for f in "$top"/tests/*.cpp "$top"/tests_p9/*.cpp; do
	one "$f"
done
for f in "$top"/tests_eh/*.cpp; do
	one -E "$f"
done
echo "c++ tests: $pass/$n"
[ $pass -eq $n ]
