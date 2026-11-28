#!/bin/bash
# run9legacy
#	Boot 9legacy (Plan 9 from Bell Labs, 4th edition, with the
#	9legacy patches) on amd64 under QEMU, as ../phase2/run9front.sh
#	boots 9front: the kernel is 9k's k10f (a terminal), booted
#	directly (multiboot: -kernel), its root the CD image's tree on the
#	host (work/root, from 9legacy.iso: fetch.sh) over virtio9p (#9/0),
#	and this repository a second virtio9p device (#9/1), mounted on
#	/n/ottd by the tree's termrc.local, which then runs $script (rc,
#	a path in the repository; default: say what the system is) and
#	reboots, which ends QEMU.  Its output is work/out.txt; the
#	console's, work/console.log.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/.." && pwd)
w=$here/work
root=$w/root
[ -f "$root/amd64/9k10f.elf" ] || { echo "run9legacy: no $root: run fetch.sh" >&2; exit 1; }
script=${script:-phase5/probe.rc}
rm -f "$w/out.txt"
cat >"$root/rc/bin/termrc.local" <<EOF
#!/bin/rc
# written by run9legacy.sh
bind -a '#9' /dev
mount -c '#9/1' /n/ottd
rc /n/ottd/$script >/n/ottd/phase5/work/out.txt >[2=1]
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
	-append "nobootprompt=virtio9p user=glenda console=0 b115200 *nomp=1" \
	-fsdev local,id=root,path="$root",security_model=none \
	-device virtio-9p-pci,disable-legacy=on,fsdev=root,mount_tag=root \
	-fsdev local,id=repo,path="$repo",security_model=none \
	-device virtio-9p-pci,disable-legacy=on,fsdev=repo,mount_tag=ottd \
	-netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
	-display none -serial file:"$w/console.log" || true
[ -f "$w/out.txt" ] || { echo "run9legacy: no output; console in $w/console.log" >&2; tail -20 "$w/console.log" >&2; exit 1; }
cat "$w/out.txt"
