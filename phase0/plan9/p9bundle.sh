#!/bin/bash
# p9bundle
#	Pack the translated tests in out/p9 with the shims and
#	run9front.rc into out/p9bundle, for a run on real 9front:
#	run_9front.sh, or copy it to any 9front machine and run
#		rc run9front.rc >[2=1] | tee results.txt
#	Run run_p9.sh and run_compcert.sh first.
#
#	tests/name.c		translated test
#	tests/name.expected	native stdout and stderr
#	tests/name.rc		native exit status
set -euo pipefail
. "$(dirname "$0")/../common.sh"

b=$out/p9bundle
rm -rf "$b"
mkdir -p "$b/tests" "$b/shim" "$b/Results"
cp "$top/plan9/rt_p9.c" "$top/plan9/libc_p9.c" "$top/plan9/run9front.rc" "$b"
cp "$top"/plan9/shim/* "$b/shim"
# knucleotide reads its input relative to the current directory
cp "$goken/benchs/compcert/Results/knucleotide-input.txt" "$b/Results"

n=0
for d in "$out"/p9/*/; do
	t=$(basename "$d")
	[ -f "$d/t_p9.c" ] && [ -f "$d/ref.rc" ] || continue
	cp "$d/t_p9.c" "$b/tests/$t.c"
	cp "$d/ref.out" "$b/tests/$t.expected"
	cp "$d/ref.rc" "$b/tests/$t.rc"
	n=$((n+1))
done
echo "$b: $n tests"
