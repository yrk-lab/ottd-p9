#!/bin/bash
# p9check [-x clangflags] [-e expected] file.cpp|file.c
#	Translate one test and build it with kencc:
#	clang -> LLVM IR (C++: + libc++) -> internalize, globaldce
#	-> llvm-cbe -cbe-explicit-padding -> lower.py --libc
#	-> 6c -> 6l with rt_p9, libc_p9 and goken's libc.
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
mkdir -p "$d"

say()
{
	echo "$name: $*"
}

# compile a shim when its source is newer than its object
shim()
{
	if [ ! "$out/p9/$1.6" -nt "$plan9/$1.c" ]; then
		6c -w -Damd64 $inc -o "$out/p9/$1.6" "$plan9/$1.c" || fatal "cannot compile $1.c"
	fi
}

translate()
{
	case $src in
	*.cpp)
		[ -f "$out/libcxx_min.ll" ] || fatal "no libc++ IR: run build_libcxx.sh"
		clang++-17 $cxxflags $xflags -S -emit-llvm "$src" -o "$d/t.ll" &&
		llvm-link "$d/t.ll" "$out/libcxx_min.ll" -S -o "$d/all.ll" ;;
	*.c)
		clang-17 $cflags $xflags -S -emit-llvm "$src" -o "$d/all.ll" ;;
	esac &&
	opt-17 -passes=internalize,globaldce --internalize-public-api-list=main "$d/all.ll" -S -o "$d/pruned.ll" &&
	"$cbe" -cbe-explicit-padding "$d/pruned.ll" -o "$d/t.c" &&
	python3 "$plan9/lower.py" --libc <"$d/t.c" >"$d/t_p9.c"
}

native()
{
	case $src in
	*.cpp)	clang++-17 --target=$triple -std=c++20 -stdlib=libc++ -fno-exceptions -fno-rtti -O1 $xflags "$src" -o "$d/ref" ;;
	*.c)	clang-17 $cflags $xflags "$src" -o "$d/ref" -lm ;;
	esac
}

# run prog tag: output in $d/tag.out, exit status in $d/tag.rc
run()
{
	(cd "$srcdir" && timeout 300 "$1") </dev/null >"$d/$2.out" 2>&1
	echo "rc=$?" >"$d/$2.rc"
}

shim rt_p9
shim libc_p9
rm -f "$d/t_p9.6" "$d/t.exe" "$d"/p9.* "$d"/ref.*

if ! translate >"$d/translate.log" 2>&1; then
	say TRANSLATION FAILED
	tail -3 "$d/translate.log"
	exit 1
fi
if ! (cd "$d" && 6c -w -I "$plan9/shim" t_p9.c) >"$d/6c.log" 2>&1; then
	say 6c FAILED
	grep -v '^warning' "$d/6c.log" | sed 5q
	exit 1
fi
if ! (cd "$d" && 6l -L "$goken/ROOT/arch/amd64/lib" -H7 -E _main -o t.exe t_p9.6 ../rt_p9.6 ../libc_p9.6 -lc) >"$d/6l.log" 2>&1; then
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
