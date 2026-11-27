#!/bin/bash
# run9front
#	Build and run the translated OpenTTD on real 9front, in place:
#	boot 9front diskless under QEMU (root from the 9front tree, as
#	../phase0/plan9/run_9front.sh), with this repository served by a
#	second u9fs and mounted on /n/ottd, and run build.rc at boot.
#	build.rc compiles work/tu (buildtu.sh's translation) with mk
#	SYS=9front, so a boot compiles only what changed, and nothing is
#	copied: the data, base set and regression save are read where
#	they are, the results written to work/9f.  Run buildtu.sh and
#	../phase1/gate.sh run ... tr first.
#
#	Checked:
#	- the socket tests (../phase0/tests_net), translated, against
#	  their output on Linux;
#	- the gate scenarios: their saves, normalized, go to
#	  ../phase1/out/9front and are compared with the reference
#	  build's (ref: the stock build, ../phase1/build_ref.sh);
#	- with gui=1, the Plan 9 video driver on the bare screen (VGA, no
#	  rio) for $guitime seconds (default 120): the screen (topng) is
#	  work/9front-screen.t.png $guishots seconds after the draw shim's
#	  window is up (default 20).  With guiinput=script, input is
#	  sent through QEMU's monitor once the window is up, a line
#	  "seconds command" at a time: "at x y" moves the mouse to x y
#	  (PS/2 is relative: to the corner first, then in small steps),
#	  "click" presses and releases the left button, anything else
#	  goes to the monitor (sendkey right 2000; sendkey grave_accent);
#	- a dedicated server: once it is up, the stock Linux build joins
#	  it as a client through QEMU's port forward (host port 13979)
#	  for $clienttime seconds (default 60); the server must report the
#	  client joined and was not dropped, and neither side a desync.
#
#	While it runs, ./vmsh is a shell in the VM and ./vmfs reads its
#	namespace (/proc, /net, /dev/kmesg...) with plan9port's 9p; the
#	QEMU monitor is work/qemu.mon (screendump file.ppm: the screen).
#	With keep=1 the VM stays up after build.rc, for a look around:
#	./vmsh 'echo reboot >/dev/reboot' ends it.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
out= work=
. "$here/../phase0/common.sh"
p0=$top
repo=$(realpath "$here/..")
p1=$repo/phase1/work
root=$nine/root
f9=$here/work/9f
[ -f "$here/work/tu/mkfile" ] || fatal "no work/tu: run buildtu.sh"
[ -x "$nine/u9fs" ] && [ -d "$root/rc" ] || fatal "no 9front in $nine: run ../phase0/plan9/setup_9front.sh"

# the socket tests: translated as ../phase0/plan9/p9check.sh does (each
# written only if it changed), and their output on Linux
t=$here/work/nettest
mkdir -p "$t"
for f in "$p0"/tests_net/*.c; do
	n=$(basename "${f%.c}")
	s=/dev/shm/nettest.$$
	clang-17 $cflags -S -emit-llvm "$f" -o "$s.ll" &&
	opt-17 -passes=internalize,globaldce --internalize-public-api-list=main "$s.ll" -S -o "$s.pruned.ll" &&
	"$cbe" -cbe-explicit-padding "$s.pruned.ll" -o "$s.cbe.c" &&
	python3 "$p0/plan9/lower.py" --libc <"$s.cbe.c" >"$s.c" &&
	gcc -O1 -U_FORTIFY_SOURCE -o "$s.native" "$f" ||
		fatal "cannot translate $f"
	cmp -s "$s.c" "$t/$n.c" || cp "$s.c" "$t/$n.c"
	timeout 60 "$s.native" >"$t/$n.expected" 2>&1
	rm -f "$s".*
done

# what build.rc runs, and where it leaves the results
rm -rf "${f9:?}"
mkdir -p "$f9/ded"
printf '[misc]\nsavegame_format = none\n[gui]\nautosave = off\n[network]\nserver_name = 9front\n' \
	>"$f9/ded/openttd.cfg"
if [ -n "${gui:-}" ]; then
	mkdir -p "$f9/gui"
	printf '[misc]\nsavegame_format = none\n[gui]\nautosave = off\n' >"$f9/gui/openttd.cfg"
fi

mkdir -p "$root/n/ottd" "$root/n/ottdfs""$here/work/tu/obj.9front"
cat >"$root/rc/bin/termrc.local" <<'RC'
#!/bin/rc
# termrc.local for phase2/run9front.sh: on a boot with p9test=ottd,
# mount the repository, build and run the translated OpenTTD, then
# power off.  A diskless boot has no loopback interface: add it, as
# termrc does on a disk.
if(~ $p9test ottd){
	if(! grep -s '^127\.0\.0\.1 ' /net/ipselftab)
		ip/ipconfig -P loopback /dev/null 127.1
	# the screen (vgasize, monitor, mouseport from the boot line):
	# termrc sets it up after termrc.local
	screenrc
	srv -nc tcp!10.0.2.100!565 ottd /n/ottdfs
	# a way in, for the host (vmsh, vmfs): rc, and the namespace by
	# exportfs, on ports QEMU forwards from this machine only
	aux/listen1 -t tcp!*!17019 /bin/rc &
	aux/listen1 -t tcp!*!17564 /bin/exportfs -s &
	bind /n/ottdfs/ottd /n/ottd
	objtype=amd64 NPROC=2 ticks=TICKS guitime=GUITIME guishots=(GUISHOTS) netdebug=NETDEBUG rc /n/ottd/phase2/build.rc >/n/ottd/phase2/work/9f/results.txt >[2=1]
	cat /n/ottd/phase2/work/9f/results.txt
	if(! ~ KEEP 1)
		echo reboot >/dev/reboot
}
RC
sed -i "s/TICKS/${ticks:-100000}/; s/GUITIME/${guitime:-120}/; s/GUISHOTS/${guishots:-20}/; s/NETDEBUG/${netdebug:-3}/; s/KEEP/${keep:-0}/" "$root/rc/bin/termrc.local"

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
# u9fs chroots to the tree it serves and looks users up in its
# etc/passwd: the repository is served bind-mounted on ottd in a tree
# that has one (the guest binds /n/ottdfs/ottd on /n/ottd)
rr=$here/work/9fsroot
mkdir -p "$rr/etc" "$rr/ottd"
cp "$root/etc/passwd" "$root/etc/group" "$rr/etc/"
printf '#!/bin/sh\nmount --bind %s %s/ottd && exec %s/u9fs -a none -u %s %s\n' \
	"$repo" "$rr" "$nine" "$(id -un)" "$rr" >"$here/work/u9repo"
chmod +x "$here/work/u9repo"
u9repo="unshare -rm $here/work/u9repo"
fwd="hostfwd=tcp:127.0.0.1:13979-:3979,hostfwd=udp:127.0.0.1:13979-:3979"
fwd="$fwd,hostfwd=tcp:127.0.0.1:17019-:17019,hostfwd=tcp:127.0.0.1:17564-:17564"
$wrap timeout ${timeout:-7200} qemu-system-x86_64 $accel -m 4G -smp 2 -nographic -no-reboot \
	-kernel "$nine/9pc64" \
	-append "nobootprompt=tcp fs=$fs auth=$fs console=0 b115200 user=glenda p9test=ottd vgasize=1024x768x32 monitor=vesa mouseport=ps2" \
	-netdev "user,id=net0,guestfwd=tcp:$fs:564-cmd:$u9fs,guestfwd=tcp:$fs:565-cmd:$u9repo,$fwd" \
	-device e1000,netdev=net0 \
	-monitor "unix:$here/work/qemu.mon,server=on,wait=off" \
	>"$here/work/9front-console.log" 2>&1 &
qemu=$!

# the monitor: mon command...
mon()
{
	echo "$*" | socat - "UNIX-CONNECT:$here/work/qemu.mon" >/dev/null 2>&1
}

# the mouse, relative, by steps of at most 10: move dx dy
move()
{
	local dx=$1 dy=$2 sx sy
	while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
		sx=$(( dx > 10 ? 10 : (dx < -10 ? -10 : dx) ))
		sy=$(( dy > 10 ? 10 : (dy < -10 ? -10 : dy) ))
		mon mouse_move $sx $sy
		dx=$((dx - sx))
		dy=$((dy - sy))
	done
}

# play the input script, timed from the draw shim's window
play()
{
	local start now t cmd
	while kill -0 $qemu 2>/dev/null && [ ! -f "$f9/gui/shot.ready" ]; do
		sleep 1
	done
	start=$(date +%s.%N)
	while read -r t cmd; do
		case $t in ''|'#'*) continue ;; esac
		now=$(date +%s.%N)
		sleep "$(echo "$start + $t - $now" | bc | sed 's/^-.*/0/')"
		echo "$t: $cmd"
		set -- $cmd
		case $1 in
		at)
			move -2000 -2000
			move $2 $3
			;;
		click)
			mon mouse_button 1
			sleep 0.2
			mon mouse_button 0
			;;
		*)
			mon "$cmd"
			;;
		esac
	done <"$guiinput"
}
player=
if [ -n "${gui:-}" ] && [ -n "${guiinput:-}" ]; then
	guiinput=$(realpath "$guiinput")
	play >"$here/work/input.log" 2>&1 &
	player=$!
fi

# the client, once the server is up
client=
while kill -0 $qemu 2>/dev/null && [ ! -f "$f9/ded/ready" ]; do
	sleep 5
done
if [ -f "$f9/ded/ready" ]; then
	c=$here/work/client
	rm -rf "$c"
	mkdir -p "$c/data/openttd" "$c/config"
	ln -s "$p1/data/openttd/baseset" "$c/data/openttd/baseset"
	printf '[misc]\nsavegame_format = none\n[gui]\nautosave = off\n[network]\nclient_name = host\n' \
		>"$c/openttd.cfg"
	XDG_DATA_HOME=$c/data XDG_CONFIG_HOME=$c/config timeout ${clienttime:-60} \
		"$p1/build-ref/openttd" -n 127.0.0.1:13979 -x -c "$c/openttd.cfg" \
		-v null:ticks=1000000000 -s null -m null -d net=${netdebug:-3} >"$c/log" 2>&1
	client=$c/log
	touch "$f9/ded/done"
fi
wait $qemu
[ -z "$player" ] || kill $player 2>/dev/null

[ -f "$f9/results.txt" ] || fatal "no results; console log in $here/work/9front-console.log"
cat "$f9/results.txt"

fail=0
echo "== socket tests"
for f in "$t"/*.expected; do
	n=$(basename "${f%.expected}")
	if cmp -s "$f" "$f9/$n.out"; then
		echo "$n: PASS"
	else
		echo "$n: DIFFERENT"
		diff "$f" "$f9/$n.out" | sed 20q
		fail=1
	fi
done

echo "== gate"
o=$repo/phase1/out/9front
rm -rf "${o:?}"
mkdir -p "$o"
for s in world regression; do
	[ -f "$f9/gate/$s.sav" ] &&
		python3 "$repo/phase1/savenorm.py" "$f9/gate/$s.sav" "$o/$s.sav"
done
"$repo/phase1/gate.sh" cmp ref 9front || fail=1

if [ -n "${gui:-}" ]; then
	echo "== video driver"
	rm -f "$here"/work/9front-screen*.png
	if ls "$f9"/gui/screen.*.png >/dev/null 2>&1; then
		for f in "$f9"/gui/screen.*.png; do
			t=${f%.png}
			t=${t##*.}
			cp "$f" "$here/work/9front-screen.$t.png" &&
				echo "screenshot: $here/work/9front-screen.$t.png"
		done
		python3 "$repo/phase3/sheet.py" "$here/work/9front-screens.png" "$here"/work/9front-screen.*.png 2>/dev/null &&
			echo "all: $here/work/9front-screens.png"
	else
		echo "no screenshot"
		tail -10 "$f9/gui/log" 2>/dev/null
		fail=1
	fi
fi

echo "== dedicated server"
srv=$f9/ded/log
if [ -n "$client" ] && grep -q 'has joined the game' "$srv" && grep -q 'has started a new company' "$srv" &&
	! grep -qi 'desync' "$srv" "$client" && ! grep -q 'is dropped\|made an error' "$srv"; then
	echo "client joined, stayed, no desync: PASS"
else
	echo "dedicated server: FAILED"
	[ -f "$srv" ] && tail -10 "$srv"
	[ -n "$client" ] && tail -10 "$client"
	fail=1
fi
exit $fail
