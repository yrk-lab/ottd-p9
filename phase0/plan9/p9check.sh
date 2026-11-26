#!/bin/bash
# p9check [-x clangflags] [-e expected] file.cpp|file.c
#	Translate one test and build it with kencc:
#	clang -> LLVM IR (C++: + libc++/libc++abi) -> internalize, globaldce
#	-> C++: exception lowering (lowereh) -> llvm-cbe -cbe-explicit-padding
#	-> lower.py --libc -> 6c -> 6l with rt_p9, libc_p9, for C++ the
#	exception runtime eh_rt and the generated wrappers, and goken's libc.
#	Run it and a native clang build in the source's directory, and
#	compare stdout+stderr and exit status; with -e, also compare the
#	output with the file expected.  Prints "name: PASS" or the reason
#	it failed, and exits accordingly.
set -uo pipefail
. "$(dirname "$0")/../common.sh"
gokenpath

usage()
{
	fatal "usage: p9check [-x clangflags] [-e expected] file.cpp|file.c"
}

xflags=
expected=
while getopts x:e: c; do
	case $c in
	x)	xflags=$OPTARG ;;
	e)	expected=$OPTARG ;;
	*)	usage ;;
	esac
done
shift $((OPTIND-1))
[ $# -eq 1 ] || usage

plan9=$top/plan9
src=$(realpath "$1")
srcdir=$(dirname "$src")
name=$(basename "${src%.*}")
d=$out/p9/$name
inc="-I$goken/include -I$goken/include/ALL -I$goken/include/arch/amd64"
ehinc="-I$top/eh -I$top/eh/plan9"
cpp=
case $src in
*.cpp)	cpp=1 ;;
esac
mkdir -p "$d"

say()
{
	echo "$name: $*"
}

# shim dir name: compile dir/name.c when it or a header it may use
# (eh/, eh/plan9/) is newer than its object
shim()
{
	local f
	for f in "$1/$2.c" "$top"/eh/*.h "$top"/eh/plan9/*.h; do
		if [ ! "$out/p9/$2.6" -nt "$f" ]; then
			6c -w -Damd64 $inc $ehinc -o "$out/p9/$2.6" "$1/$2.c" || fatal "cannot compile $2.c"
			return
		fi
	done
}

translate()
{
	if [ -n "$cpp" ]; then
		clang++-17 $cxxflags $xflags -S -emit-llvm "$src" -o "$d/t.ll" &&
		linkcxx "$d/all.bc" "$d/t.ll" &&
		opt-17 -passes=internalize,globaldce --internalize-public-api-list=main,eh_can_catch \
			"$d/all.bc" -S -o "$d/pruned.ll" &&
		lowereh "$d/pruned.ll" "$d/eh.ll" "$d/invoke.c" &&
		"$cbe" -cbe-explicit-padding "$d/eh.ll" -o "$d/t.c"
	else
		clang-17 $cflags $xflags -S -emit-llvm "$src" -o "$d/all.ll" &&
		opt-17 -passes=internalize,globaldce --internalize-public-api-list=main \
			"$d/all.ll" -S -o "$d/pruned.ll" &&
		"$cbe" -cbe-explicit-padding "$d/pruned.ll" -o "$d/t.c"
	fi &&
	python3 "$plan9/lower.py" --libc <"$d/t.c" >"$d/t_p9.c"
}

native()
{
	if [ -n "$cpp" ]; then
		clang++-17 $cxxflags $nativeld $xflags "$src" -o "$d/ref"
	else
		clang-17 $cflags $nativeld $xflags "$src" -o "$d/ref" -lm
	fi
}

# run prog tag: output in $d/tag.out, exit status in $d/tag.rc
run()
{
	(cd "$srcdir" && timeout 300 "$1") </dev/null >"$d/$2.out" 2>&1
	echo "rc=$?" >"$d/$2.rc"
}

# cc file flags...: compile file in $d with 6c, log appended to 6c.log
cc()
{
	local f=$1
	shift
	if ! (cd "$d" && 6c -w "$@" "$f") >>"$d/6c.log" 2>&1; then
		say 6c FAILED on "$f"
		grep -v '^warning' "$d/6c.log" | sed 5q
		exit 1
	fi
}

shim "$plan9" rt_p9
shim "$plan9" libc_p9
shim "$top/eh" eh_rt
rm -f "${d:?}"/*.6 "$d/t.exe" "$d"/p9.* "$d"/ref.* "$d/invoke.c" "$d/6c.log"

if ! translate >"$d/translate.log" 2>&1; then
	say TRANSLATION FAILED
	tail -3 "$d/translate.log"
	exit 1
fi
objs="t_p9.6 ../rt_p9.6 ../libc_p9.6"
if [ -n "$cpp" ]; then
	cc invoke.c -Damd64 $inc $ehinc
	objs="$objs invoke.6 ../eh_rt.6"
fi
cc t_p9.c -I "$plan9/shim"
if ! (cd "$d" && 6l -L "$goken/ROOT/arch/amd64/lib" -H7 -E _main -o t.exe $objs -lc) >"$d/6l.log" 2>&1; then
	say 6l FAILED
	sed 5q "$d/6l.log"
	exit 1
fi
if ! native >"$d/native.log" 2>&1; then
	say NATIVE BUILD FAILED
	tail -3 "$d/native.log"
	exit 1
fi
run "$d/t.exe" p9
run "$d/ref" ref

if ! cmp -s "$d/ref.out" "$d/p9.out" || ! cmp -s "$d/ref.rc" "$d/p9.rc"; then
	say MISMATCH
	diff "$d/ref.out" "$d/p9.out" | sed 6q
	echo "	native $(cat "$d/ref.rc"), 6c $(cat "$d/p9.rc")"
	exit 1
fi
if [ -n "$expected" ] && ! cmp -s "$d/p9.out" "$expected"; then
	say "MISMATCH with $expected (native agrees with 6c)"
	exit 1
fi
say PASS
