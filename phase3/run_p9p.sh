#!/bin/bash
# run_p9p [openttd args...]
#	Fast loop for the Plan 9 video driver: the translated OpenTTD,
#	unit by unit (../phase2/buildtu.sh), built for Linux (mk
#	SYS=linux: the system's C library, obj.linux) with the real draw
#	shim (../phase0/plan9/draw_p9.c, with kbdp9p.c) on plan9port's
#	libdraw, and run with -v plan9: a window on this machine's
#	display, through devdraw.  Only what changed is translated and
#	compiled again.  Needs plan9port in $PLAN9 (default ~/plan9) and
#	$DISPLAY.
#
#	For tests, input=script plays a script once the window is up: each
#	line "seconds xdotool-command", %w standing for the window, seconds
#	counted from the window's start (as the screenshots), e.g.
#		20 mousemove --window %w 532 458 click 1
#	shots="t..." (default 20) are the draw shim's screenshots,
#	work/shot.t.ppm, made PNG as work/shot.t.png.  The game is stopped
#	$runtime seconds (default 60) after the window is up.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../phase1/common.sh"
export PLAN9=${PLAN9:-$HOME/plan9}
[ -x "$PLAN9/bin/9c" ] || fatal "no plan9port in $PLAN9"
[ -z "${input:-}" ] || input=$(realpath "$input")
sys=linux "$here/../phase2/buildtu.sh" || exit 1
o=$here/../phase2/work/tu/obj.linux

# a personal directory with the base set, as phase1/gate.sh
c=$here/work/run
mkdir -p "$c/data/openttd" "$c/config"
ln -sfn "$data/openttd/baseset" "$c/data/openttd/baseset"
[ -f "$c/openttd.cfg" ] || printf '[misc]\nsavegame_format = none\n[gui]\nautosave = off\n' >"$c/openttd.cfg"
cd "$o" || exit 1
if [ -z "${input:-}${shots:-}" ]; then
	XDG_DATA_HOME=$c/data XDG_CONFIG_HOME=$c/config ./openttd -x -c "$c/openttd.cfg" -v plan9 -s "${sound:-null}" -m null "$@"
	exit
fi

w=$here/work
rm -f "$w"/shot.*
XDG_DATA_HOME=$c/data XDG_CONFIG_HOME=$c/config p9drawshot=$w/shot p9drawshotat="${shots:-20}" \
	./openttd -x -c "$c/openttd.cfg" -v plan9 -s "${sound:-null}" -m null "$@" >"$w/log" 2>&1 &
pid=$!
for i in $(seq 60); do
	[ -f "$w/shot.ready" ] && break
	sleep 1
done
[ -f "$w/shot.ready" ] || { kill $pid; tail "$w/log"; fatal "no window"; }
start=$(date +%s.%N)
win=$(xdotool search --pid $pid 2>/dev/null | tail -1)
[ -n "$win" ] || win=$(xdotool search --name '^OpenTTD' | tail -1)
if [ -n "${input:-}" ]; then
	while read -r t cmd; do
		case $t in ''|'#'*) continue ;; esac
		now=$(date +%s.%N)
		sleep "$(echo "$start + $t - $now" | bc | sed 's/^-.*/0/')"
		echo "$t: $cmd"
		xdotool ${cmd//%w/$win}
	done <"$input"
fi
now=$(date +%s.%N)
sleep "$(echo "$start + ${runtime:-60} - $now" | bc | sed 's/^-.*/0/')"
kill $pid 2>/dev/null
wait $pid 2>/dev/null
for f in "$w"/shot.*.ppm; do
	[ -f "$f" ] && python3 "$here/ppm2png.py" "$f" "${f%.ppm}.png" && echo "screenshot: ${f%.ppm}.png"
done
