#!/bin/bash
# One test through: clang -> IR -> prune -> llvm-cbe(-cbe-explicit-padding) -> lower.py -> kencc 6c -> 6l + goken libc -> run; diff vs native.
#   G=/path/to/goken9cc W=/path/to/work ./p9check.sh ../tests/t3_stl.cpp
set -uo pipefail; P=$(cd "$(dirname "$0")" && pwd); . "$P/../common.sh"
G=${G:-$W/goken9cc}; export PATH=$G/bin:$G/ROOT/arch/boot-gcc/bin:$PATH objtype=amd64
src=$1; t=$(basename "$src" .cpp); d=$OUT/p9/$t; mkdir -p "$d"; INC="-I$G/include -I$G/include/ALL -I$G/include/arch/amd64"
[ -f "$OUT/p9/rt_p9.6" ] || 6c -w -Damd64 $INC -o "$OUT/p9/rt_p9.6" "$P/rt_p9.c" || exit 2
clang++-17 $CXXFLAGS -S -emit-llvm "$src" -o "$d/t.ll" &&
llvm-link "$d/t.ll" "$OUT/libcxx_min.ll" -S -o "$d/all.ll" &&
opt-17 -passes='internalize,globaldce' --internalize-public-api-list=main "$d/all.ll" -S -o "$d/pruned.ll" &&
"$CBE" -cbe-explicit-padding "$d/pruned.ll" -o "$d/t.c" &&
python3 "$P/lower.py" < "$d/t.c" | perl -pe 's/\bprintf\b/print/g' > "$d/t_p9.c" || { echo "$t: TRANSLATE FAIL"; exit 1; }
( cd "$d" && rm -f t_p9.6 t.exe p9.out
  6c -w -I "$P/shim" t_p9.c > 6c.log 2>&1; grep -v '^warning' 6c.log | head -5
  6l -L "$G/ROOT/arch/amd64/lib" -H7 -E _main -o t.exe t_p9.6 ../rt_p9.6 -lc > 6l.log 2>&1; head -3 6l.log
  [ -x t.exe ] && ./t.exe > p9.out 2>&1; echo "rc=$?" >> p9.out )
clang++-17 --target=$TRIPLE -std=c++20 -stdlib=libc++ -fno-exceptions -fno-rtti -O1 "$src" -o "$d/ref" && "$d/ref" > "$d/ref.out"; echo "rc=$?" >> "$d/ref.out"
if cmp -s "$d/ref.out" "$d/p9.out"; then echo "$t: PASS (kencc output == native)"; else echo "$t: MISMATCH"
  echo " native: $(head -3 "$d/ref.out" | tr '\n' '|')"; echo " 6c    : $(head -3 "$d/p9.out" | tr '\n' '|')"; exit 1; fi
