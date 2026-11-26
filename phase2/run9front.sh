#!/bin/bash
# run9front
#	Build and run the translated OpenTTD on real 9front: stage the
#	units of build9.sh (work/p9/u) and the Plan 9 runtime sources in
#	the 9front root as /ottd, boot it diskless (as ../phase0/plan9/
#	run_9front.sh), run build.rc at boot, power off and print
#	/ottd/results.txt.  Run build9.sh and ../phase1/gate.sh run ... tr
#	first.
#
#	Staged and checked:
#	- the socket tests (../phase0/tests_net), translated, against
#	  their output on Linux;
#	- the game data, base set and regression save: build.rc plays
#	  the gate scenarios, and their saves, normalized, go to
#	  ../phase1/out/9front and are compared with the translated Linux
#	  build's (tr);
#	- a dedicated server: once it is up, the stock Linux build joins
#	  it as a client through QEMU's port forward (host port 13979) for
#	  $clienttime seconds (default 60); the server must report the
#	  client joined, and neither side a desync.
#	With only=net, just the runtime and the socket tests (minutes).
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
out= work=
. "$here/../phase0/common.sh"
p0=$top
u=$here/work/p9/u
p1=$here/../phase1/work
root=$nine/root
only=${only:-}
[ -n "$only" ] || [ -f "$u/openttd.h" ] || fatal "no $u: run build9.sh"
[ -x "$nine/u9fs" ] && [ -d "$root/rc" ] || fatal "no 9front in $nine: run ../phase0/plan9/setup_9front.sh"

rm -rf "${root:?}/ottd"
mkdir -p "$root/ottd/shim" "$root/ottd/nettest"
cp "$p0/plan9/rt_p9.c" "$p0/plan9/libc_p9.c" "$p0/plan9/os_p9.c" "$p0/plan9/net_p9.c" \
	"$p0/plan9/net9f.c" "$p0/plan9/net.h" "$p0/eh/eh_rt.c" "$root/ottd/"
cp "$p0"/plan9/shim/*.h "$p0/eh/eh_rt.h" "$p0/eh/plan9/eh_sys.h" "$root/ottd/shim/"
cp "$here/build.rc" "$root/ottd/"

# the socket tests: translated as ../phase0/plan9/p9check.sh does, and
# their output on Linux
t=$here/work/nettest
mkdir -p "$t"
for f in "$p0"/tests_net/*.c; do
	n=$(basename "${f%.c}")
	clang-17 $cflags -S -emit-llvm "$f" -o "$t/$n.ll" &&
	opt-17 -passes=internalize,globaldce --internalize-public-api-list=main "$t/$n.ll" -S -o "$t/$n.pruned.ll" &&
	"$cbe" -cbe-explicit-padding "$t/$n.pruned.ll" -o "$t/$n.cbe.c" &&
	python3 "$p0/plan9/lower.py" --libc <"$t/$n.cbe.c" >"$root/ottd/nettest/$n.c" &&
	gcc -O1 -U_FORTIFY_SOURCE -o "$t/$n.native" "$f" ||
		fatal "cannot translate $f"
	timeout 60 "$t/$n.native" >"$t/$n.expected" 2>&1
done

if [ -z "$only" ]; then
	cp "$u"/openttd.h "$u"/data.c "$u"/fn*.c "$u"/invoke.c "$root/ottd/"
	for d in lang ai game baseset media bin; do
		cp -rL "$p1/build-p9/$d" "$root/ottd/$d"
	done
	cp -rL "$p1/data/openttd/baseset" "$root/ottd/opengfx"
	cp "$p1/regression.sav" "$root/ottd/"
	mkdir -p "$root/ottd/ded"
	printf '[misc]\nsavegame_format = none\n[gui]\nautosave = off\n[network]\nserver_name = 9front\n' \
		>"$root/ottd/ded/openttd.cfg"
fi

cat >"$root/rc/bin/termrc.local" <<'RC'
#!/bin/rc
# termrc.local for phase2/run9front.sh: on a boot with p9test=ottd,
# build and run the translated OpenTTD, then power off.  A diskless
# boot has no loopback interface: add it, as termrc does on a disk.
if(~ $p9test ottd){
	if(! grep -s '^127\.0\.0\.1 ' /net/ipselftab)
		ip/ipconfig -P loopback /dev/null 127.1
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
fwd="hostfwd=tcp:127.0.0.1:13979-:3979,hostfwd=udp:127.0.0.1:13979-:3979"
$wrap timeout ${timeout:-7200} qemu-system-x86_64 $accel -m 4G -smp 2 -nographic -no-reboot \
	-kernel "$nine/9pc64" \
	-append "nobootprompt=tcp fs=$fs auth=$fs console=0 b115200 user=glenda p9test=ottd" \
	-netdev "user,id=net0,guestfwd=tcp:$fs:564-cmd:$u9fs,$fwd" \
	-device e1000,netdev=net0 \
	>"$here/work/9front-console.log" 2>&1 &
qemu=$!

# the client, once the server is up
client=
if [ -z "$only" ]; then
	while kill -0 $qemu 2>/dev/null && [ ! -f "$root/ottd/ded/ready" ]; do
		sleep 5
	done
	if [ -f "$root/ottd/ded/ready" ]; then
		c=$here/work/client
		rm -rf "$c"
		mkdir -p "$c/data/openttd" "$c/config"
		ln -s "$p1/data/openttd/baseset" "$c/data/openttd/baseset"
		printf '[misc]\nsavegame_format = none\n[gui]\nautosave = off\n[network]\nclient_name = host\n' \
			>"$c/openttd.cfg"
		XDG_DATA_HOME=$c/data XDG_CONFIG_HOME=$c/config timeout ${clienttime:-60} \
			"$p1/build-ref/openttd" -n 127.0.0.1:13979 -x -c "$c/openttd.cfg" \
			-v null:ticks=1000000000 -s null -m null -d net=3 >"$c/log" 2>&1
		client=$c/log
		touch "$root/ottd/ded/done"
	fi
fi
wait $qemu

[ -f "$root/ottd/results.txt" ] || fatal "no results; console log in $here/work/9front-console.log"
cp "$root/ottd/results.txt" "$here/work/9front-results.txt"
cat "$here/work/9front-results.txt"

fail=0
echo "== socket tests"
for f in "$t"/*.expected; do
	n=$(basename "${f%.expected}")
	if cmp -s "$f" "$root/ottd/nettest/$n.out"; then
		echo "$n: PASS"
	else
		echo "$n: DIFFERENT"
		diff "$f" "$root/ottd/nettest/$n.out" | sed 20q
		fail=1
	fi
done
[ -z "$only" ] || exit $fail

echo "== gate"
o=$here/../phase1/out/9front
rm -rf "${o:?}"
mkdir -p "$o"
for s in world regression; do
	[ -f "$root/ottd/gate/$s.sav" ] &&
		python3 "$here/../phase1/savenorm.py" "$root/ottd/gate/$s.sav" "$o/$s.sav"
done
"$here/../phase1/gate.sh" cmp tr 9front || fail=1

echo "== dedicated server"
srv=$root/ottd/ded/log
if [ -n "$client" ] && grep -q 'has joined the game' "$srv" && grep -q 'has started a new company' "$srv" &&
	! grep -qi 'desync' "$srv" "$client"; then
	echo "client joined, no desync: PASS"
else
	echo "dedicated server: FAILED"
	[ -f "$srv" ] && tail -10 "$srv"
	[ -n "$client" ] && tail -10 "$client"
	fail=1
fi
exit $fail
