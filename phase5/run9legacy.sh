#!/bin/bash
# run9legacy
#	Boot 9legacy (Plan 9 from Bell Labs, 4th edition, with the
#	9legacy patches) on amd64 under QEMU, as ../phase2/run9front.sh
#	boots 9front: the kernel is 9k's k10f (a terminal), booted
#	directly (multiboot: -kernel), its root the CD image's tree on the
#	host (work/root, from 9legacy.iso: fetch.sh) over virtio9p (#9/0),
#	and this repository a second virtio9p device (#9/1), mounted on
#	/n/ottd by the tree's termrc.local, which then runs $script (rc,
#	a path in the repository; default: say what the system is; with
#	$ticks, default 100) and reboots, which ends QEMU; saves the script
#	leaves in work/out (build.rc: the gate's) are compared with the
#	reference build's.  Its output is work/out.txt; the
#	console's, work/console.log.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/.." && pwd)
w=$here/work
root=$w/root
[ -f "$root/amd64/9k10f.elf" ] || { echo "run9legacy: no $root: run fetch.sh" >&2; exit 1; }
script=${script:-phase5/probe.rc}
# what build.rc writes in: the translated tree's objects (FreeType's go
# in its own tree; 9front builds in a copy, freetype9f)
mkdir -p "$repo/phase2/work/tu/obj.9legacy" "$repo/phase0/out/tu/t2/obj.9legacy"
rm -f "$w/out.txt"
cat >"$root/rc/bin/termrc.local" <<EOF
#!/bin/rc
# written by run9legacy.sh
bind -a '#9' /dev
mount -c '#9/1' /n/ottd
# the network, which termrc would set up after this: the ether, the
# connection server, DHCP from QEMU, the loopback
bind -a '#l0' /net >[2]/dev/null
ndb/cs -f /lib/ndb/local
ip/ipconfig >[2]/dev/null
ip/ipconfig loopback /dev/null 127.1
# a shell for ../phase2/vmsh (host port 17019), as run9front.sh's
aux/listen1 -t 'tcp!*!17019' /bin/rc &
ticks=${ticks:-100} gui=${gui:-0} guiaudio=${guiaudio:-0} guirio=${guirio:-0} guifocus=(${guifocus:-}) guitime=${guitime:-60} guishots=(${guishots:-30}) rc /n/ottd/$script >/n/ottd/phase5/work/out.txt >[2=1]
if(! ~ ${keep:-0} 1)
	echo reboot >'#c/reboot'
EOF
# KVM where this user may use it (as run9front.sh), else TCG
accel=
wrap=
if [ -w /dev/kvm ]; then
	accel="-enable-kvm -cpu host"
elif id -nG "$(id -un)" | grep -qw kvm && sudo -n true 2>/dev/null; then
	accel="-enable-kvm -cpu host"
	wrap="sudo -n setpriv --reuid=$(id -u) --regid=$(id -g) --init-groups"
fi
$wrap timeout ${timeout:-600} qemu-system-x86_64 $accel -m 4G -smp 2 -no-reboot \
	-kernel "$root/amd64/9k10f.elf" \
	-append "nobootprompt=virtio9p user=glenda console=0 b115200 *nomp=1 monitor=vesa vgasize=1024x768x32 mouseport=ps2" \
	-fsdev local,id=root,path="$root",security_model=none \
	-device virtio-9p-pci,disable-legacy=on,fsdev=root,mount_tag=root \
	-fsdev local,id=repo,path="$repo",security_model=none \
	-device virtio-9p-pci,disable-legacy=on,fsdev=repo,mount_tag=ottd \
	-netdev user,id=net0,hostfwd=tcp:127.0.0.1:13979-:3979,hostfwd=udp:127.0.0.1:13979-:3979,hostfwd=tcp:127.0.0.1:17019-:17019 \
	-device virtio-net-pci,netdev=net0 \
	-monitor "unix:$w/qemu.mon,server=on,wait=off" \
	-display none -serial file:"$w/console.log" &
qemu=$!

# input for the GUI (guiinput=script), through QEMU's monitor, as
# run9front.sh sends it: "seconds command" lines from when the window
# is up (build.rc's gui/shot.ready); "at x y" moves the mouse there
# (PS/2 is relative: to the corner, then in steps), "click" clicks,
# anything else is a monitor command (sendkey right)
mon()
{
	echo "$*" | socat - "UNIX-CONNECT:$w/qemu.mon" >/dev/null 2>&1
}
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
play()
{
	local start now t cmd
	while kill -0 $qemu 2>/dev/null && [ ! -f "$w/out/gui/shot.ready" ]; do
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
rm -f "$w/out/gui/shot.ready"
player=
if [ -n "${guiinput:-}" ]; then
	guiinput=$(realpath "$guiinput")
	play >"$w/input.log" 2>&1 &
	player=$!
fi
# a dedicated server (build.rc says when it is up, in work/out/ded):
# the stock Linux build joins it through the port forward for
# $clienttime seconds, as run9front.sh's client does
p1=$repo/phase1/work
rm -f "$w/out/ded/ready" "$w/out/ded/done"
client=
while kill -0 $qemu 2>/dev/null && [ ! -f "$w/out/ded/ready" ]; do
	sleep 5
done
if [ -f "$w/out/ded/ready" ]; then
	c=$w/client
	rm -rf "$c"
	mkdir -p "$c/data/openttd" "$c/config"
	ln -s "$p1/data/openttd/baseset" "$c/data/openttd/baseset"
	printf '[misc]\nsavegame_format = none\nprefer_sprite_font = true\n[gui]\nautosave = off\n[network]\nclient_name = host\n' \
		>"$c/openttd.cfg"
	XDG_DATA_HOME=$c/data XDG_CONFIG_HOME=$c/config timeout ${clienttime:-60} \
		"$p1/build-ref/openttd" -n 127.0.0.1:13979 -x -c "$c/openttd.cfg" \
		-v null:ticks=1000000000 -s null -m null -d net=3 >"$c/log" 2>&1 || true	# timeout ends it
	client=$c/log
	touch "$w/out/ded/done"
fi
wait $qemu || true
[ -z "$player" ] || kill $player 2>/dev/null
[ -f "$w/out.txt" ] || { echo "run9legacy: no output; console in $w/console.log" >&2; tail -20 "$w/console.log" >&2; exit 1; }
cat "$w/out.txt"
# build.rc's gate saves, normalized, against the reference build's
# (../phase1/gate.sh), as run9front.sh compares 9front's
if ls "$w"/out/*.sav >/dev/null 2>&1; then
	echo "== gate"
	o=$repo/phase1/out/9legacy
	rm -rf "${o:?}"
	mkdir -p "$o"
	for s in world regression; do
		[ -f "$w/out/$s.sav" ] &&
			python3 "$repo/phase1/savenorm.py" "$w/out/$s.sav" "$o/$s.sav"
	done
	"$repo/phase1/gate.sh" cmp ref 9legacy || fail=1
fi
if [ -n "$client" ]; then
	echo "== dedicated server"
	srv=$w/out/ded/log
	if grep -q 'has joined the game' "$srv" && grep -q 'has started a new company' "$srv" &&
		! grep -qi 'desync' "$srv" "$client" && ! grep -q 'is dropped\|made an error' "$srv"; then
		echo "client joined, stayed, no desync: PASS"
	else
		echo "dedicated server: FAILED"
		tail -10 "$srv"
		tail -10 "$client"
		fail=1
	fi
fi
# build.rc's stop: the program quit on the interrupt and left no
# process behind (issue #21)
if grep -q 'quit on interrupt\|did not quit\|killed by' "$w/out.txt"; then
	echo "== stops"
	if grep -E 'killed by the interrupt|left behind|did not quit' "$w/out.txt"; then
		fail=1
	else
		echo "quit on interrupt, nothing left behind: PASS"
	fi
fi
exit ${fail:-0}
