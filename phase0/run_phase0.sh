#!/bin/bash
# run_phase0 [test...]
#	Phase 0 gate: tests/*.cpp (C++20, exceptions, RTTI) -> LLVM IR
#	+ libc++/libc++abi IR -> internalize, globaldce -> exception
#	lowering -> llvm-cbe -> gcc (standing in for the Plan 9 compiler)
#	with eh/eh_rt.c and the generated wrappers -> run.  The output
#	must equal that of the native clang build.  Logs in out/test/.
set -uo pipefail
. "$(dirname "$0")/common.sh"

# The native reference build must link, or every test would fail
# for reasons that have nothing to do with the translator.
preflight()
{
	printf '#include <cstdio>\nint main(){std::puts("ok");}\n' >"$out/pre.cpp"
	if ! clang++-17 $cxxflags $nativeld "$out/pre.cpp" -o "$out/pre" >"$out/pre.log" 2>&1; then
		tail -5 "$out/pre.log" >&2
		fatal "cannot link a plain clang++ -stdlib=libc++ program; try:
	sudo apt-get install -y clang-17 libc++-17-dev libc++abi-17-dev libunwind-17-dev"
	fi
}

# step name cmd...: run cmd, output in $d/name.log
step()
{
	stage=$1
	shift
	"$@" >"$d/$stage.log" 2>&1
}

# compare: run both builds, compare output and exit status
compare()
{
	"$d/t.ref" >"$d/ref.out" 2>&1
	echo rc=$? >>"$d/ref.out"
	"$d/t.cbe" >"$d/cbe.out" 2>&1
	echo rc=$? >>"$d/cbe.out"
	cmp "$d/ref.out" "$d/cbe.out"
}

# check test: the whole pipeline; on failure stage names the step
check()
{
	src=$top/tests/$1.cpp
	d=$out/$1
	mkdir -p "$d"
	step clang clang++-17 $cxxflags -S -emit-llvm "$src" -o "$d/t.ll" &&
	step link linkcxx "$d/all.bc" "$d/t.ll" &&
	step prune opt-17 -passes=internalize,globaldce --internalize-public-api-list=main,eh_can_catch \
		"$d/all.bc" -S -o "$d/pruned.ll" &&
	step lower lowereh "$d/pruned.ll" "$d/eh.ll" "$d/invoke.c" &&
	step cbe "$cbe" "$d/eh.ll" -o "$d/t.cbe.c" &&
	step cc gcc -w -O1 -std=gnu11 -fno-builtin -I"$top/eh" -I"$top/eh/unix" \
		"$d/t.cbe.c" "$d/invoke.c" "$top/eh/eh_rt.c" "$top/rt.c" -lm -o "$d/t.cbe" &&
	step ref clang++-17 $cxxflags $nativeld "$src" -o "$d/t.ref" &&
	step run compare
}

mkdir -p "$out"
[ -f "$libir/stamp" ] || "$top/build_libcxx.sh" || exit 1
preflight

tests=${*:-$(cd "$top/tests" && ls *.cpp | sed 's/\.cpp$//')}
pass=0
fail=0
rows=()
for t in $tests; do
	if check $t; then
		pass=$((pass+1))
		res=PASS
	else
		fail=$((fail+1))
		res="FAIL@$stage"
		echo "--- $t failed at $stage (log: $out/$t/$stage.log)"
		tail -5 "$out/$t/$stage.log"
	fi
	lines=$(wc -l <"$out/$t/t.cbe.c" 2>/dev/null || echo -)
	rows+=("$(printf '%-14s %-12s C lines: %s' $t "$res" "$lines")")
done

echo
echo "== Phase 0 results =="
printf '%s\n' "${rows[@]}"
echo
"$top/audit.py" "$out"/*/t.cbe.c 2>/dev/null | grep -v '^$'
echo
if [ $fail -ne 0 ]; then
	echo "GATE 0: FAIL ($fail failed)"
	exit 1
fi
echo "GATE 0: PASS ($pass/$pass)"
