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
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../phase1/common.sh"
export PLAN9=${PLAN9:-$HOME/plan9}
[ -x "$PLAN9/bin/9c" ] || fatal "no plan9port in $PLAN9"
sys=linux "$here/../phase2/buildtu.sh" || exit 1
o=$here/../phase2/work/tu/obj.linux

# a personal directory with the base set, as phase1/gate.sh
c=$here/work/run
mkdir -p "$c/data/openttd" "$c/config"
ln -sfn "$data/openttd/baseset" "$c/data/openttd/baseset"
[ -f "$c/openttd.cfg" ] || printf '[misc]\nsavegame_format = none\n[gui]\nautosave = off\n' >"$c/openttd.cfg"
cd "$o" || exit 1
XDG_DATA_HOME=$c/data XDG_CONFIG_HOME=$c/config ./openttd -x -c "$c/openttd.cfg" -v plan9 -s null -m null "$@"
