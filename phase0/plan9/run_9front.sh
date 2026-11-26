#!/bin/bash
# Phase 0.5 on REAL 9front, headless: boot the 9front kernel diskless in QEMU with its root file system served from the
# host by u9fs (pattern from yrk-lab/rd CI), run out/p9bundle/run9front.rc at boot, power off, report.
#   one-time: plan9/setup_9front.sh        each run: plan9/p9bundle.sh && plan9/run_9front.sh
# QEMU guestfwd (on 10.0.2.100; slirp refuses its own gateway 10.0.2.2) starts u9fs per 9P connection on stdin/stdout,
# inside an unprivileged user namespace (unshare -r) so its chroot works: no sudo, socat or port 564 on the host.
set -uo pipefail; P=$(cd "$(dirname "$0")" && pwd); . "$P/../common.sh"
N=${NINE:-$HOME/9front}; R=$N/root; B=$OUT/p9bundle
[ -x "$N/u9fs" ] && [ -f "$N/9pc64" ] && [ -d "$R/rc" ] || { echo "no 9front root in $N: run plan9/setup_9front.sh" >&2; exit 2; }
[ -f "$B/run9front.rc" ] || { echo "no bundle: run plan9/p9bundle.sh" >&2; exit 2; }
rm -rf "$R/p9bundle"; cp -r "$B" "$R/p9bundle"; rm -f "$R/p9bundle/exitcode" "$R/p9bundle/results.txt"
cat > "$R/rc/bin/termrc.local" <<'RC'
#!/bin/rc
# Only on boots started by run_9front.sh (kernel arg p9test=yes); a normal boot of this root is unaffected.
if(~ $p9test yes){
	cd /p9bundle
	rc run9front.rc >results.txt >[2=1]
	if(~ $status '') echo 0 >exitcode
	if not echo 1 >exitcode
	cat results.txt
	echo reboot >/dev/reboot
}
RC
ACCEL="-enable-kvm -cpu host"; [ -w /dev/kvm ] || ACCEL=""
timeout "${TIMEOUT:-1200}" qemu-system-x86_64 $ACCEL -m 2G -smp 2 -nographic -no-reboot \
  -kernel "$N/9pc64" \
  -append "nobootprompt=tcp fs=10.0.2.100 auth=10.0.2.100 console=0 b115200 user=glenda p9test=yes" \
  -netdev "user,id=n0,guestfwd=tcp:10.0.2.100:564-cmd:unshare -r $N/u9fs -a none -u $(id -un) $R" -device e1000,netdev=n0 \
  > "$OUT/9front-console.log" 2>&1
echo "qemu exit $? (console log: $OUT/9front-console.log)"
cp "$R/p9bundle/results.txt" "$OUT/9front-results.txt" 2>/dev/null && cat "$OUT/9front-results.txt" || { echo "no results; console tail:"; tail -30 "$OUT/9front-console.log"; exit 1; }
[ "$(cat "$R/p9bundle/exitcode" 2>/dev/null)" = 0 ]
