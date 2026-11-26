#!/bin/bash
# One ISO C program through: clang(C) -> IR -> prune -> llvm-cbe(-cbe-explicit-padding) -> lower.py -> kencc 6c
#   -> 6l + goken libc + libc_p9 shim -> run; compare against native clang -O1 and (if given) an expected-output file.
#   G=... W=... ./p9check_c.sh prog.c [expected.txt] [stdin-file]
set -uo pipefail; P=$(cd "$(dirname "$0")" && pwd); . "$P/../common.sh"
G=${G:-$W/goken9cc}; export PATH=$G/bin:$G/ROOT/arch/boot-gcc/bin:$PATH objtype=amd64
src=$1; exp=${2:-}; in=${3:-/dev/null}; t=$(basename "$src" .c); d=$OUT/p9c/$t; mkdir -p "$d"
INC="-I$G/include -I$G/include/ALL -I$G/include/arch/amd64"
CFLAGS_C="--target=$TRIPLE -std=gnu11 -fgnu89-inline -O1 -g0 -w ${XCFLAGS:-}"
RUN=$(cd "$(dirname "$src")" && pwd)   # programs may open data files relative to their source dir
# C runtime shim: libc_p9.c (ISO C names on Plan 9 libc). Rebuilt when the source is newer.
for f in libc_p9; do
  [ "$OUT/p9c/$f.6" -nt "$P/$f.c" ] || 6c -w -Damd64 $INC -o "$OUT/p9c/$f.6" "$P/$f.c" || exit 2
done
clang-17 $CFLAGS_C -S -emit-llvm "$src" -o "$d/t.ll" &&
opt-17 -passes='internalize,globaldce' --internalize-public-api-list=main "$d/t.ll" -S -o "$d/pruned.ll" &&
"$CBE" -cbe-explicit-padding "$d/pruned.ll" -o "$d/t.c" &&
python3 "$P/lower.py" --libc < "$d/t.c" > "$d/t_p9.c" || { echo "$t: TRANSLATE FAIL"; exit 1; }
( cd "$d" && rm -f t_p9.6 t.exe p9.out p9.rc
  6c -w -I "$P/shim" t_p9.c > 6c.log 2>&1 || { echo "$t: 6c FAIL"; grep -v '^warning' 6c.log | head -5; }
  [ -f t_p9.6 ] && { 6l -L "$G/ROOT/arch/amd64/lib" -H7 -E _main -o t.exe t_p9.6 ../libc_p9.6 -lc > 6l.log 2>&1 \
     || { echo "$t: 6l FAIL"; head -5 6l.log; }; }
  [ -x t.exe ] && { (cd "$RUN" && timeout 300 "$d/t.exe") < "$in" > p9.out 2>&1; echo "rc=$?" > p9.rc; } )
clang-17 $CFLAGS_C "$src" -o "$d/ref" -lm && (cd "$RUN" && timeout 300 "$d/ref") < "$in" > "$d/ref.out" 2>&1; echo "rc=$?" > "$d/ref.rc"
[ -f "$d/p9.out" ] || exit 1
if ! cmp -s "$d/ref.out" "$d/p9.out" || ! cmp -s "$d/ref.rc" "$d/p9.rc"; then echo "$t: MISMATCH vs native"
  diff "$d/ref.out" "$d/p9.out" | head -6; echo "   native $(cat "$d/ref.rc"), 6c $(cat "$d/p9.rc")" | sed 's/^/   /'; exit 1; fi
if [ -n "$exp" ] && ! cmp -s "$d/p9.out" "$exp"; then echo "$t: MISMATCH vs expected (native agrees with 6c)"; exit 1; fi
echo "$t: PASS (kencc output == native${exp:+ == expected})"
