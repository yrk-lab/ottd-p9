#!/bin/bash
# p9check [-E] [-x clangflags] [-e expected] file.cpp|file.c
#	Translate one test and build it with kencc:
#	clang -> LLVM IR (C++: + libc++) -> internalize, globaldce
#	-> llvm-cbe -cbe-explicit-padding -> lower.py --libc
#	-> 6c -> 6l with rt_p9, libc_p9 and goken's libc.
#	Run it and a native clang build in the source's directory, and
#	compare stdout+stderr and exit status; with -e, also compare the
#	output with the file expected.  Prints "name: PASS" or the reason
#	it failed, and exits accordingly.
#	-E	C++ with exceptions and RTTI: link the libc++/libc++abi IR
#		built by run_eh.sh, lower exceptions (Emscripten style) and
#		add the eh/ runtime and the generated __invoke_ wrappers.
set -uo pipefail
. "$(dirname "$0")/../common.sh"
gokenpath

usage()
{
	fatal "usage: p9check [-E] [-x clangflags] [-e expected] file.cpp|file.c"
}

xflags=
expected=
eh=
while getopts Ex:e: c; do
	case $c in
	E)	eh=1 ;;
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
lib=$out/libcxx_min.ll
publics=main
if [ -n "$eh" ]; then
	cxxflags="--target=$triple -std=c++20 -stdlib=libc++ -O1 -g0 -D_LIBCPP_DISABLE_EXTERN_TEMPLATE"
	lib=$out/eh/libcxx_eh.ll
	publics=main,eh_can_catch
fi
mkdir -p "$d"

say()
{
	echo "$name: $*"
}

# shim dir name: compile dir/name.c when it is newer than its object
shim()
{
	if [ ! "$out/p9/$2.6" -nt "$1/$2.c" ]; then
		6c -w -Damd64 $inc $ehinc -o "$out/p9/$2.6" "$1/$2.c" || fatal "cannot compile $2.c"
	fi
}

translate()
{
	case $src in
	*.cpp)
		[ -f "$lib" ] || fatal "no $lib: run build_libcxx.sh (run_eh.sh for -E)"
		clang++-17 $cxxflags $xflags -S -emit-llvm "$src" -o "$d/t.ll" &&
		llvm-link "$d/t.ll" "$lib" -S -o "$d/all.ll" ;;
	*.c)
		clang-17 $cflags $xflags -S -emit-llvm "$src" -o "$d/all.ll" ;;
	esac &&
	opt-17 -passes=internalize,globaldce --internalize-public-api-list=$publics "$d/all.ll" -S -o "$d/pruned.ll" &&
	lowereh &&
	"$cbe" -cbe-explicit-padding "$d/pruned.ll" -o "$d/t.c" &&
	python3 "$plan9/lower.py" --libc <"$d/t.c" >"$d/t_p9.c"
}

# exceptions: invokes become __invoke_ wrapper calls (see eh/eh_rt.h)
lowereh()
{
	[ -n "$eh" ] || return 0
	opt-17 -wasm-lower-em-ehsjlj -enable-emscripten-cxx-exceptions "$d/pruned.ll" -S -o "$d/eh.ll" &&
	sed 's/ thread_local / /; s/ cc99 / /' "$d/eh.ll" >"$d/pruned.ll" &&
	python3 "$top/eh/mkinvoke.py" <"$d/pruned.ll" >"$d/invoke.c"
}

native()
{
	case $src in
	*.cpp)
		if [ -n "$eh" ]; then
			clang++-17 $cxxflags $xflags "$src" -o "$d/ref"
		else
			clang++-17 --target=$triple -std=c++20 -stdlib=libc++ -fno-exceptions -fno-rtti -O1 $xflags "$src" -o "$d/ref"
		fi ;;
	*.c)	clang-17 $cflags $xflags "$src" -o "$d/ref" -lm ;;
	esac
}

# run prog tag: output in $d/tag.out, exit status in $d/tag.rc
run()
{
	(cd "$srcdir" && timeout 300 "$1") </dev/null >"$d/$2.out" 2>&1
	echo "rc=$?" >"$d/$2.rc"
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
if [ -n "$eh" ]; then
	objs="$objs invoke.6 ../eh_rt.6"
	if ! (cd "$d" && 6c -w -Damd64 $inc $ehinc invoke.c) >"$d/6c.log" 2>&1; then
		say 6c FAILED on invoke.c
		grep -v '^warning' "$d/6c.log" | sed 5q
		exit 1
	fi
fi
if ! (cd "$d" && 6c -w -I "$plan9/shim" t_p9.c) >>"$d/6c.log" 2>&1; then
	say 6c FAILED
	grep -v '^warning' "$d/6c.log" | sed 5q
	exit 1
fi
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
