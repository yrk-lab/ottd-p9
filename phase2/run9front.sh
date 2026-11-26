#!/bin/bash
# run9front
#	Build and run the translated OpenTTD on real 9front: stage the
#	units of build9.sh (work/p9/u) and the Plan 9 runtime sources in
#	the 9front root as /ottd, boot it diskless (as ../phase0/plan9/
#	run_9front.sh), run build.rc at boot, power off and print
#	/ottd/results.txt.  The game data and the gate's base set and
#	regression save are staged too: build.rc plays the gate
#	scenarios, and their saves, normalized, go to ../phase1/out/9front
#	and are compared with the translated Linux build's (tr).
#	Run build9.sh and ../phase1/gate.sh run ... tr first.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
out= work=
. "$here/../phase0/common.sh"
p0=$top
u=$here/work/p9/u
root=$nine/root
[ -f "$u/openttd.h" ] || fatal "no $u: run build9.sh"
[ -x "$nine/u9fs" ] && [ -d "$root/rc" ] || fatal "no 9front in $nine: run ../phase0/plan9/setup_9front.sh"

rm -rf "${root:?}/ottd"
mkdir -p "$root/ottd/shim"
cp "$u"/openttd.h "$u"/data.c "$u"/fn*.c "$u"/invoke.c "$root/ottd/"
cp "$p0/plan9/rt_p9.c" "$p0/plan9/libc_p9.c" "$p0/plan9/os_p9.c" "$p0/eh/eh_rt.c" "$root/ottd/"
cp "$p0"/plan9/shim/*.h "$p0/eh/eh_rt.h" "$p0/eh/plan9/eh_sys.h" "$root/ottd/shim/"
cp "$here/build.rc" "$root/ottd/"
p1=$here/../phase1/work
for d in lang ai game baseset media bin; do
	cp -rL "$p1/build-p9/$d" "$root/ottd/$d"
done
cp -rL "$p1/data/openttd/baseset" "$root/ottd/opengfx"
cp "$p1/regression.sav" "$root/ottd/"
cat >"$root/rc/bin/termrc.local" <<'RC'
#!/bin/rc
# termrc.local for phase2/run9front.sh: on a boot with p9test=ottd,
# build and run the translated OpenTTD, then power off.
if(~ $p9test ottd){
	ticks=TICKS rc /ottd/build.rc >/ottd/results.txt >[2=1]
	cat /ottd/results.txt
	echo reboot >/dev/reboot
}
RC
sed -i "s/TICKS/${ticks:-100000}/" "$root/rc/bin/termrc.local"

accel=
wrap=
if [ -w /dev/kvm ]; then
	accel="-enable-kvm -cpu host"
elif id -nG "$(id -un)" | grep -qw kvm && sudo -n true 2>/dev/null; then
	accel="-enable-kvm -cpu host"
	wrap="sudo -n setpriv --reuid=$(id -u) --regid=$(id -g) --init-groups"
fi
fs=10.0.2.100
u9fs="unshare -r $nine/u9fs -a none -u $(id -un) $root"
$wrap timeout ${timeout:-7200} qemu-system-x86_64 $accel -m 4G -smp 2 -nographic -no-reboot \
	-kernel "$nine/9pc64" \
	-append "nobootprompt=tcp fs=$fs auth=$fs console=0 b115200 user=glenda p9test=ottd" \
	-netdev "user,id=net0,guestfwd=tcp:$fs:564-cmd:$u9fs" \
	-device e1000,netdev=net0 \
	>"$here/work/9front-console.log" 2>&1
[ -f "$root/ottd/results.txt" ] || fatal "no results; console log in $here/work/9front-console.log"
cp "$root/ottd/results.txt" "$here/work/9front-results.txt"
cat "$here/work/9front-results.txt"

o=$here/../phase1/out/9front
rm -rf "${o:?}"
mkdir -p "$o"
for s in world regression; do
	[ -f "$root/ottd/gate/$s.sav" ] &&
		python3 "$here/../phase1/savenorm.py" "$root/ottd/gate/$s.sav" "$o/$s.sav"
done
"$here/../phase1/gate.sh" cmp tr 9front
