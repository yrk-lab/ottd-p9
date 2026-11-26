#!/bin/bash
# run_p9
#	Phase 0.5 fast loop: every C++ test in tests/ and tests_p9/
#	through p9check (translator + goken 6c on Linux).
#	Run plan9/setup_goken.sh once first.
set -uo pipefail
. "$(dirname "$0")/../common.sh"

[ -f "$libir/stamp" ] || "$top/build_libcxx.sh" || exit 1

pass=0
n=0
for f in "$top"/tests/*.cpp "$top"/tests_p9/*.cpp; do
	n=$((n+1))
	r=$("$top/plan9/p9check.sh" "$f")
	echo "$r"
	case $r in
	*": PASS")	pass=$((pass+1)) ;;
	esac
done
echo "c++ tests: $pass/$n"
[ $pass -eq $n ]
