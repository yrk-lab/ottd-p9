#!/bin/bash
# gate run openttd name
# gate cmp name1 name2
#	Phase 1 gate.  run plays each scenario with the null video
#	driver (one thread, no network, as fast as possible) for $ticks
#	ticks, saves uncompressed on exit, and keeps the save, with its
#	random session id blanked (savenorm.py), as out/name/scenario.sav.
#	cmp compares two runs byte for byte.
#
#	scenarios:
#	world		new game from seed $seed (map generation, towns,
#			industries, economy)
#	regression	upstream's regression/regression/test.sav,
#			loaded without AIs (its vehicles keep running);
#			upstream's is xz-compressed and the builds have no
#			liblzma: work/regression.sav is it uncompressed (unxz.py)
set -uo pipefail
. "$(dirname "$0")/common.sh"

ticks=${ticks:-100000}	# about 3.7 game years
seed=${seed:-1234567}
scenarios="world regression"

usage()
{
	fatal "usage: gate run openttd name | gate cmp name1 name2"
}

# play scenario: run one scenario in a fresh personal directory
play()
{
	local s=$1 d=$out/$name/$1
	rm -rf "$d"
	mkdir -p "$d/data/openttd" "$d/config" "$d/lib"
	ln -s "$data/openttd/baseset" "$d/data/openttd/baseset"
	# the Plan 9 builds' data is $HOME/lib/openttd, the stock build's
	# XDG_DATA_HOME/openttd: one directory
	ln -s ../data/openttd "$d/lib/openttd"
	cat >"$d/openttd.cfg" <<-EOF
	[misc]
	savegame_format = none
	prefer_sprite_font = true
	[gui]
	autosave = off
	autosave_on_exit = true
	[game_creation]
	town_name = english
	[ai_players]
	none =
	EOF

	local game=(-g)
	case $s in
	world)		game=(-G "$seed" -g) ;;
	regression)
		[ -f "$work/regression.sav" ] ||
			python3 "$top/unxz.py" "$ref/regression/regression/test.sav" "$work/regression.sav" || return 1
		game=(-g "$work/regression.sav") ;;
	esac

	HOME=$d XDG_DATA_HOME=$d/data XDG_CONFIG_HOME=$d/config \
	timeout 1800 "$bin" -x -c "$d/openttd.cfg" -v "null:ticks=$ticks" -s null -m null "${game[@]}" \
		>"$d/log" 2>&1
	local st=$?
	local sav=$d/save/autosave/exit.sav	# personal dir is the config file's
	if [ $st -ne 0 ] || [ ! -f "$sav" ]; then
		echo "$name/$s: FAILED (status $st)"
		tail -5 "$d/log"
		return 1
	fi
	python3 "$top/savenorm.py" "$sav" "$out/$name/$s.sav" || return 1
	echo "$name/$s: $(md5sum <"$out/$name/$s.sav" | cut -c1-32) $(wc -c <"$sav") bytes"
}

case ${1:-} in
run)
	[ $# -eq 3 ] || usage
	bin=$(realpath "$2")
	name=$3
	fail=0
	for s in $scenarios; do
		play $s || fail=1
	done
	exit $fail
	;;
cmp)
	[ $# -eq 3 ] || usage
	fail=0
	for s in $scenarios; do
		if cmp -s "$out/$2/$s.sav" "$out/$3/$s.sav"; then
			echo "$s: same"
		else
			echo "$s: DIFFERENT"
			cmp "$out/$2/$s.sav" "$out/$3/$s.sav" | sed 1q
			fail=1
		fi
	done
	exit $fail
	;;
*)
	usage
	;;
esac
