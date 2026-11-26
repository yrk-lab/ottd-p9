#!/bin/bash
# run_eh [test...]
#	C++ exceptions through the translator: tests_eh/*.cpp compiled
#	with exceptions and RTTI -> LLVM IR + libc++ and libc++abi IR
#	(built with exceptions) -> internalize, globaldce -> Emscripten
#	exception lowering (opt -wasm-lower-em-ehsjlj) -> llvm-cbe -> gcc
#	with eh/eh_rt.c and the generated eh/ wrappers -> run.  The output
#	must equal that of the native clang build.
set -uo pipefail
. "$(dirname "$0")/common.sh"

ehflags="--target=$triple -std=c++20 -stdlib=libc++ -O1 -g0 -D_LIBCPP_DISABLE_EXTERN_TEMPLATE"
lib=$out/eh/libcxx_eh.ll

# step name cmd...: run cmd, output in $d/name.log
step()
{
	stage=$1
	shift
	"$@" >"$d/$stage.log" 2>&1
}

lower()
{
	opt-17 -wasm-lower-em-ehsjlj -enable-emscripten-cxx-exceptions "$d/pruned.ll" -S -o "$d/eh.ll" &&
	# no threads, and the wrappers are plain C functions
	sed -i 's/ thread_local / /; s/ cc99 / /' "$d/eh.ll" &&
	python3 "$top/eh/mkinvoke.py" <"$d/eh.ll" >"$d/invoke.c"
}

compare()
{
	"$d/t.ref" >"$d/ref.out" 2>&1
	echo rc=$? >>"$d/ref.out"
	"$d/t.cbe" >"$d/cbe.out" 2>&1
	echo rc=$? >>"$d/cbe.out"
	cmp "$d/ref.out" "$d/cbe.out"
}

check()
{
	src=$top/tests_eh/$1.cpp
	d=$out/eh/$1
	mkdir -p "$d"
	step clang clang++-17 $ehflags -S -emit-llvm "$src" -o "$d/t.ll" &&
	step link llvm-link "$d/t.ll" "$lib" -S -o "$d/all.ll" &&
	step prune opt-17 -passes=internalize,globaldce --internalize-public-api-list=main,eh_can_catch \
		"$d/all.ll" -S -o "$d/pruned.ll" &&
	step lower lower &&
	step cbe "$cbe" "$d/eh.ll" -o "$d/t.c" &&
	step cc gcc -w -O1 -std=gnu11 -DEH -I"$top/eh" -I"$top/eh/unix" "$d/t.c" "$d/invoke.c" "$top/eh/eh_rt.c" "$top/rt.c" -o "$d/t.cbe" &&
	step ref clang++-17 $ehflags "$src" -o "$d/t.ref" &&
	step run compare
}

mkdir -p "$out/eh"
if [ ! "$lib" -nt "$top/eh/eh_match.cpp" ]; then
	"$top/build_libcxx.sh" eh >"$out/eh/lib.log" 2>&1 || { tail -5 "$out/eh/lib.log"; fatal "cannot build libc++/libc++abi IR"; }
fi

tests=${*:-$(cd "$top/tests_eh" && ls *.cpp | sed 's/\.cpp$//')}
pass=0
n=0
for t in $tests; do
	n=$((n+1))
	if check $t; then
		pass=$((pass+1))
		echo "$t: PASS"
	else
		echo "$t: FAIL at $stage (log: $out/eh/$t/$stage.log)"
		tail -5 "$out/eh/$t/$stage.log"
	fi
done
echo "eh: $pass/$n"
[ $pass -eq $n ]
