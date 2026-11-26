#!/bin/bash
# run_9front
#	Phase 0.5 on real 9front.  Boot the 9front kernel diskless in
#	QEMU with its root served from this machine by u9fs (as in the
#	rd CI), run out/p9bundle at boot (termrc.local), power off and
#	print the results.  Run setup_9front.sh once, p9bundle.sh first.
#
#	QEMU's guestfwd starts u9fs for each 9P connection, on
#	10.0.2.100: slirp will not forward its own gateway, 10.0.2.2.
#	u9fs chroots into the served tree, so it runs in a user
#	namespace (unshare -r): no root, socat or port 564 on the host.
set -uo pipefail
. "$(dirname "$0")/../common.sh"

root=$nine/root
b=$out/p9bundle
[ -x "$nine/u9fs" ] && [ -f "$nine/9pc64" ] && [ -d "$root/rc" ] ||
	fatal "no 9front in $nine: run plan9/setup_9front.sh"
[ -f "$b/run9front.rc" ] || fatal "no bundle: run plan9/p9bundle.sh"

rm -rf "$root/p9bundle"
cp -r "$b" "$root/p9bundle"
cp "$top/plan9/termrc.local" "$root/rc/bin/termrc.local"

# Use KVM when we can open /dev/kvm.  Just after being added to group
# kvm the login session does not have the group yet: setpriv
# --init-groups picks it up (needs sudo).
accel=
wrap=
if [ -w /dev/kvm ]; then
	accel="-enable-kvm -cpu host"
elif id -nG "$(id -un)" | grep -qw kvm && sudo -n true 2>/dev/null; then
	accel="-enable-kvm -cpu host"
	wrap="sudo -n setpriv --reuid=$(id -u) --regid=$(id -g) --init-groups"
else
	echo "no access to /dev/kvm: emulating, this takes an hour or more" >&2
fi

fs=10.0.2.100
u9fs="unshare -r $nine/u9fs -a none -u $(id -un) $root"
$wrap timeout ${timeout:-7200} qemu-system-x86_64 $accel -m 2G -smp 2 -nographic -no-reboot \
	-kernel "$nine/9pc64" \
	-append "nobootprompt=tcp fs=$fs auth=$fs console=0 b115200 user=glenda p9test=yes" \
	-netdev "user,id=net0,guestfwd=tcp:$fs:564-cmd:$u9fs" \
	-device e1000,netdev=net0 \
	>"$out/9front-console.log" 2>&1

if [ ! -f "$root/p9bundle/results.txt" ]; then
	tail -20 "$out/9front-console.log"
	fatal "no results; console log in $out/9front-console.log"
fi
cp "$root/p9bundle/results.txt" "$out/9front-results.txt"
cat "$out/9front-results.txt"
[ "$(cat "$root/p9bundle/exitcode" 2>/dev/null)" = 0 ]
