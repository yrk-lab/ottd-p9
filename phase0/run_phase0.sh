#!/bin/bash
# Phase 0 gate: C++ -> LLVM IR -> llvm-cbe -> C -> (gcc stand-in) -> run; output must equal the native build.
#   ./run_phase0.sh            all tests
#   ./run_phase0.sh t3_stl     one test
set -uo pipefail; . "$(dirname "$0")/common.sh"
mkdir -p "$OUT"; [ -f "$OUT/libcxx_min.ll" ] || "$HERE/build_libcxx.sh" || exit 2

# Preflight: the native reference build (clang + libc++) must link, otherwise every test would "fail" for reasons
# unrelated to the translator. Typical fix: sudo apt-get install -y libc++-17-dev libc++abi-17-dev libunwind-17-dev
printf '#include <cstdio>\nint main(){std::puts("ok");}\n' > "$OUT/pre.cpp"
if ! clang++-17 --target=$TRIPLE -std=c++20 -stdlib=libc++ "$OUT/pre.cpp" -o "$OUT/pre" >"$OUT/pre.log" 2>&1; then
  echo "PREFLIGHT FAILED: cannot build/link a plain clang++ -stdlib=libc++ program (this is not a translator problem):"
  tail -5 "$OUT/pre.log"
  echo "Try: sudo apt-get install -y clang-17 libc++-17-dev libc++abi-17-dev libunwind-17-dev"; exit 3
fi
TESTS=${*:-$(cd "$HERE/tests" && ls *.cpp | sed 's/\.cpp$//')}
pass=0; fail=0; rows=()
for t in $TESTS; do
  d=$OUT/$t; mkdir -p "$d"; src=$HERE/tests/$t.cpp; stage=""
  step() { stage=$1; shift; "$@" >"$d/$stage.log" 2>&1; }
  {
    step 1-clang   clang++-17 $CXXFLAGS -S -emit-llvm "$src" -o "$d/t.ll" &&
    step 2-link    llvm-link "$d/t.ll" "$OUT/libcxx_min.ll" -S -o "$d/all.ll" &&
    # keep only what main reaches; internalize so static-linkage handling gets exercised too
    step 3-prune   opt-17 -passes='internalize,globaldce' --internalize-public-api-list=main "$d/all.ll" -S -o "$d/pruned.ll" &&
    step 4-cbe     "$CBE" "$d/pruned.ll" -o "$d/t.cbe.c" &&
    step 5-cc      gcc -w -O1 -std=gnu11 "$d/t.cbe.c" "$HERE/rt.c" -o "$d/t.cbe" &&
    step 6-ref     clang++-17 --target=$TRIPLE -std=c++20 -stdlib=libc++ -fno-exceptions -fno-rtti -O1 "$src" -o "$d/t.ref" &&
    step 7-run     bash -c "'$d/t.ref' > '$d/ref.out'; echo rc=\$? >> '$d/ref.out'; '$d/t.cbe' > '$d/cbe.out'; echo rc=\$? >> '$d/cbe.out'; cmp '$d/ref.out' '$d/cbe.out'"
  }
  if [ -z "${stage:-}" ] || [ "$stage" = 7-run -a -f "$d/cbe.out" ] && cmp -s "$d/ref.out" "$d/cbe.out" 2>/dev/null; then
    pass=$((pass+1)); res=PASS
  else
    fail=$((fail+1)); res="FAIL@$stage"; echo "--- $t failed at $stage (log: $d/$stage.log)"; tail -5 "$d/$stage.log"
  fi
  rows+=("$(printf '%-14s %-12s C lines: %s' "$t" "$res" "$(wc -l < "$d/t.cbe.c" 2>/dev/null || echo -)")")
done
echo; echo "== Phase 0 results =="; printf '%s\n' "${rows[@]}"
echo; "$HERE/audit.py" "$OUT"/*/t.cbe.c 2>/dev/null | grep -v '^$'
echo; [ $fail -eq 0 ] && echo "GATE 0: PASS ($pass/$pass)" || { echo "GATE 0: FAIL ($fail failed)"; exit 1; }
