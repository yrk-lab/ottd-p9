#!/bin/bash
# build9
#	Phase 2: the translated OpenTTD (phase1/translate.sh, through the
#	lower step) compiled by kencc, into work/p9:
#	llvm-cbe -cbe-explicit-padding -> lower.py --libc -> split.py
#	(header, data.c, fnNNN.c of about 4 MB) -> goken 6c, 4 at a time
#	-> 6l with the Plan 9 runtime (rt_p9, libc_p9, os_p9, eh_rt, the
#	generated wrappers), a malloc in place of goken's placeholder
#	(malloc_goken) and goken's libc.
#	Writes work/p9/missing.txt: what the link still lacks.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
out= work=
. "$here/../phase0/common.sh"
gokenpath
p0=$top
tr=$here/../phase1/work/tr
p9=$here/work/p9
mkdir -p "$p9"

[ -f "$tr/eh.ll" ] || fatal "no $tr/eh.ll: run phase1/translate.sh ir link lower"
inc="-I$goken/include -I$goken/include/ALL -I$goken/include/arch/amd64"
ehinc="-I$p0/eh -I$p0/eh/plan9"

echo "== cbe, lower, split"
"$cbe" -cbe-explicit-padding "$tr/eh.ll" -o "$p9/openttd_pad.c" || fatal "cbe failed"
python3 "$p0/plan9/lower.py" --libc <"$p9/openttd_pad.c" >"$p9/openttd.c" || fatal "lower.py failed"
rm -rf "${p9:?}/u"
python3 "$here/split.py" "$p9/openttd.c" "$p9/u" 4 || fatal "split failed"
cp "$tr/invoke.c" "$p9/u/invoke.c"

echo "== 6c"
cd "$p9/u"
ls fn*.c data.c |
	/usr/bin/xargs -P4 -I{} sh -c "6c -w -I $p0/plan9/shim {} >{}.log 2>&1 || echo {}: 6c FAILED"
6c -w -Damd64 $inc $ehinc invoke.c || fatal "invoke.c failed"
for f in "$p0/plan9/rt_p9.c" "$p0/plan9/libc_p9.c" "$p0/plan9/os_p9.c" "$p0/plan9/malloc_goken.c" "$p0/eh/eh_rt.c"; do
	6c -w -Damd64 $inc $ehinc -o "$(basename "$f" .c).6" "$f" || fatal "$f failed"
done

echo "== 6l"
objs="data.6 fn*.6 invoke.6 rt_p9.6 libc_p9.6 os_p9.6 malloc_goken.6 eh_rt.6"
if 6l -L "$goken/ROOT/arch/amd64/lib" -H7 -E _main -o openttd.exe $objs -lc >6l.log 2>&1; then
	# the data OpenTTD finds next to its executable, as phase1/translate.sh
	for d in lang ai game baseset media bin; do
		ln -sfn "$top/../phase1/work/build-p9/$d" "$p9/u/$d"
	done
	echo "built $p9/u/openttd.exe"
	: >"$p9/missing.txt"
	exit 0
fi
# 6l stops after a few errors: compute the whole list
nm $objs 2>/dev/null | awk '$2=="U"{print $3}' | sort -u >"$p9/undef.txt"
(nm $objs 2>/dev/null; nm "$goken/ROOT/arch/amd64/lib/libc.a" 2>/dev/null) |
	awk '$2 ~ /^[TDBLR]$/{print $3}' | sort -u >"$p9/def.txt"
python3 -c "
u = set(open('$p9/undef.txt').read().split())
d = set(open('$p9/def.txt').read().split())
open('$p9/missing.txt', 'w').write(''.join(s + '\n' for s in sorted(u - d)))"
echo "6l: $(wc -l <"$p9/missing.txt") symbols missing (missing.txt)"
exit 1
