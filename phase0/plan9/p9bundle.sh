#!/bin/bash
# Collect lowered C + shims for a run on REAL 9front (the check that matters; goken is only the fast loop).
# Output: out/p9bundle/ -> copy it into your 9front VM (e.g. via 9pfs/ftp/drawterm) and run  rc run9front.rc
P=$(cd "$(dirname "$0")" && pwd); . "$P/../common.sh"; B=$OUT/p9bundle; rm -rf "$B"; mkdir -p "$B/shim"
cp "$P/rt_p9.c" "$B/"; cp "$P"/shim/* "$B/shim/"
for d in "$OUT"/p9/*/; do t=$(basename "$d"); [ -f "$d/t_p9.c" ] && cp "$d/t_p9.c" "$B/$t.c" && cp "$d/ref.out" "$B/$t.expected"; done
cat > "$B/run9front.rc" <<'RC'
#!/bin/rc
# UNTESTED on 9front (written against 6c(1)); adjust if the compiler complains. Uses 9front's own libc, not goken's.
rfork en
6c -w rt_p9.c || exit rt
fails=0
for(f in *.c){
	t=`{echo $f | sed 's/\.c$//'}
	if(~ $t rt_p9) continue
	if(! 6c -w -I shim -o $t.6 $f >[2]$t.6c.log){ echo $t: 6c FAILED; head -3 $t.6c.log; fails=1; continue }
	if(! 6l -o $t.out $t.6 rt_p9.6 >[2]$t.6l.log){ echo $t: 6l FAILED; head -3 $t.6l.log; fails=1; continue }
	./$t.out >$t.actual >[2=1]
	echo rc=0 >>$t.actual
	if(cmp -s $t.actual $t.expected) echo $t: PASS
	if not { echo $t: MISMATCH; fails=1 }
}
exit $fails
RC
echo "bundle: $B"; ls "$B"
