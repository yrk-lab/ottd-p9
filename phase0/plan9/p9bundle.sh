#!/bin/bash
# Collect lowered C + shims for a run on REAL 9front (the check that matters; goken is only the fast loop).
# Run plan9/run_p9.sh and plan9/run_compcert.sh first: this packs their translated output.
# Output: out/p9bundle/ (and out/p9bundle.tar). In 9front:  rc run9front.rc >[2=1] | tee results.txt
#   cxx/<t>.c   C++ tests (t1-t3, m1-m5), link with rt_p9.c (C++ runtime shim)
#   c/<t>.c     CompCert programs, link with libc_p9.c (ISO C on Plan 9 libc)
#   <t>.expected = native stdout+stderr, <t>.rc = native exit status
P=$(cd "$(dirname "$0")" && pwd); . "$P/../common.sh"; B=$OUT/p9bundle; rm -rf "$B"; mkdir -p "$B/shim" "$B/cxx" "$B/c" "$B/Results"
cp "$P/rt_p9.c" "$P/libc_p9.c" "$B/"; cp "$P"/shim/* "$B/shim/"
n=0
for d in "$OUT"/p9/*/; do t=$(basename "$d"); [ -f "$d/t_p9.c" ] || continue
  cp "$d/t_p9.c" "$B/cxx/$t.c"; sed '$d' "$d/ref.out" > "$B/cxx/$t.expected"; tail -1 "$d/ref.out" > "$B/cxx/$t.rc"; n=$((n+1)); done
for d in "$OUT"/p9c/*/; do t=$(basename "$d"); [ -f "$d/t_p9.c" ] || continue
  cp "$d/t_p9.c" "$B/c/$t.c"; cp "$d/ref.out" "$B/c/$t.expected"; cp "$d/ref.rc" "$B/c/$t.rc"; n=$((n+1)); done
G=${G:-$W/goken9cc}; cp "$G/benchs/compcert/Results/knucleotide-input.txt" "$B/Results/" 2>/dev/null   # knucleotide reads it
cat > "$B/run9front.rc" <<'RC'
#!/bin/rc
# Phase 0.5 on 9front: compile every translated test with the system 6c/6l and the 9front libc, run, compare
# stdout+stderr and exit status with the native Linux build.   rc run9front.rc >[2=1] | tee results.txt
# cflags/ldflags may be preset (used only for dry runs under goken on Linux); empty on 9front.
rfork e
if(~ $#cflags 0) cflags=()
if(~ $#ldflags 0) ldflags=()
pass=()
fail=()
fn note { echo $* }
fn one {
	t=$1; dir=$2; rt=$3
	if(! 6c -w $cflags -I shim -o $dir/$t.6 $dir/$t.c >$dir/$t.6c.log >[2=1]){
		note $dir/$t: 6c FAILED; grep -v '^warning' $dir/$t.6c.log | sed 5q; fail=($fail $t)
	}
	if not if(! 6l $ldflags -o $dir/$t.out $dir/$t.6 $rt >$dir/$t.6l.log >[2=1]){
		note $dir/$t: 6l FAILED; sed 5q $dir/$t.6l.log; fail=($fail $t)
	}
	if not {
		./$dir/$t.out >$dir/$t.actual >[2=1]
		st=$status
		e=`{sed 's/^rc=//' $dir/$t.rc}
		ok=no
		if(~ $e 0 && ~ $st '' 0) ok=yes
		if(! ~ $e 0 && ~ $st *$e) ok=yes
		if(! cmp -s $dir/$t.actual $dir/$t.expected) ok=no
		if(~ $ok yes){ note $dir/$t: PASS; pass=($pass $t) }
		if not { note $dir/$t: MISMATCH '(status' $"st', expected rc='$e')'; cmp $dir/$t.actual $dir/$t.expected; fail=($fail $t) }
	}
}
date
6c -w $cflags -o rt_p9.6 rt_p9.c || exit 'rt_p9.c'
6c -w $cflags -o libc_p9.6 libc_p9.c || exit 'libc_p9.c'
for(f in cxx/*.c) one `{basename $f .c} cxx rt_p9.6
for(f in c/*.c) one `{basename $f .c} c libc_p9.6
echo TOTAL: $#pass passed, $#fail failed
if(! ~ $#fail 0){ echo failed: $fail; exit fail }
exit ''
RC
( cd "$OUT" && tar cf p9bundle.tar p9bundle )
echo "bundle: $B ($n tests) -> $OUT/p9bundle.tar"
