#!/bin/bash
# translate [step...]
#	OpenTTD through the translator, into work/tr:
#	ir	every unit of build-p9 (compile_commands.json) to LLVM bitcode
#	link	link them, with what they need of the libc++/libc++abi IR
#		archive (phase0 linkcxx), into one module; internalize
#		everything but main; drop dead code
#	lower	exception lowering (phase0 lowereh): eh.ll + invoke.c
#	cbe	llvm-cbe: openttd.c
#	cc	gcc (the stand-in for the Plan 9 compiler; $ccopt, default
#		-O0: one 130 MB C file) with the phase0
#		runtime (eh, rt.c) and nothreads.c: tr/openttd
#	Default: all steps.  link writes externals.txt, the functions and
#	data the program still needs from outside: the OS layer that the
#	Plan 9 runtime has to provide.
set -uo pipefail
# phase0's settings (lowereh, linkcxx, cbe) with phase0's own paths,
# then phase1's
p0=$(cd "$(dirname "$0")/../phase0" && pwd)
out= work=
. "$p0/common.sh"
out= work=
. "$(dirname "$0")/common.sh"

tr=$work/tr
mkdir -p "$tr"

step()
{
	echo "== $1"
	do_$1 || fatal "$1 failed"
}

do_ir()
{
	[ -f "$work/build-p9/compile_commands.json" ] || fatal "no compile_commands.json: run build_p9.sh"
	python3 "$top/ir.py" "$work/build-p9/compile_commands.json" "$tr/ir"
}

do_link()
{
	[ -f "$tr/ir/units.txt" ] || fatal "no unit list: run the ir step"
	linkcxx "$tr/all.bc" $(cat "$tr/ir/units.txt") &&
	opt-17 -passes=internalize,globaldce --internalize-public-api-list=main,eh_can_catch \
		"$tr/all.bc" -o "$tr/pruned.bc" &&
	llvm-nm --undefined-only "$tr/pruned.bc" | awk '{print $NF}' | sort -u >"$tr/externals.txt" &&
	echo "$(wc -l <"$tr/ir/units.txt") units; $(llvm-nm --defined-only "$tr/pruned.bc" | wc -l) symbols defined;" \
		"$(wc -l <"$tr/externals.txt") external (externals.txt)"
}

do_lower()
{
	llvm-dis "$tr/pruned.bc" -o "$tr/pruned.ll" &&
	lowereh "$tr/pruned.ll" "$tr/eh.ll" "$tr/invoke.c"
}

do_cbe()
{
	"$cbe" "$tr/eh.ll" -o "$tr/openttd.c" &&
	echo "openttd.c: $(wc -l <"$tr/openttd.c") lines, $(du -h "$tr/openttd.c" | cut -f1)"
}

do_cc()
{
	gcc -w ${ccopt:--O0} -std=gnu11 -fno-builtin -I"$p0/eh" -I"$p0/eh/unix" \
		"$tr/openttd.c" "$tr/invoke.c" "$p0/eh/eh_rt.c" "$p0/rt.c" "$top/nothreads.c" \
		-lz -llzma -lm -o "$tr/openttd" 2>&1 | grep -v 'warning:' | head -40
	[ -x "$tr/openttd" ] || return 1
	# the data OpenTTD finds next to its executable (languages, scripts,
	# media), from the native build of the same source
	local d
	for d in lang ai game baseset media bin; do
		ln -sfn "$work/build-p9/$d" "$tr/$d"
	done
	echo "built $tr/openttd"
}

steps=${*:-ir link lower cbe cc}
for s in $steps; do
	step $s
done
