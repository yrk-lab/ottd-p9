# Settings shared by the scripts; sourced, not run.
# These may be set in the environment:
#	work	clones and builds (llvm-cbe, libc++ sources, goken9cc)
#	out	per-test output
#	cbe	the llvm-cbe binary
#	goken	goken9cc tree (Plan 9 compilers hosted on Linux)
#	nine	9front root, kernel and u9fs for run_9front.sh
#	triple	clang target

top=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ehdir=$top/eh		# the exception runtime (kept when another phase sets top)
work=${work:-$top/work}
out=${out:-$top/out}
cbe=${cbe:-$work/llvm-cbe/build/tools/llvm-cbe/llvm-cbe}
goken=${goken:-$work/goken9cc}
nine=${nine:-$HOME/9front}
triple=${triple:-x86_64-unknown-linux-gnu}	# Phase 2+: a Plan 9 triple

PATH=/usr/lib/llvm-17/bin:$PATH

# The language mode the OpenTTD build will use: C++20 with exceptions
# and RTTI.  Exceptions are lowered in the IR (lowereh) and run on the
# setjmp/longjmp runtime in eh/.  Plan 9 compilers have no 128-bit
# integers: libc++ is configured as for a target without __int128
# (std::filesystem's file_time_type then counts nanoseconds in 64 bits),
# and long double is double (-mlong-double-64), as on Plan 9: there is no
# x87 80-bit type there.  Not position independent (-fno-pic -fno-pie):
# Plan 9 binaries are not, and for PIC LLVM turns lookup tables into
# relative ones (&x - &table), which are no C constant expressions.
# Programs and the libc++ IR are compiled alike, so the ABI agrees.
cxxflags="--target=$triple -std=c++20 -stdlib=libc++ -O1 -g0 -mlong-double-64 -fno-pic -fno-pie -D_LIBCPP_DISABLE_EXTERN_TEMPLATE -D_LIBCPP_HAS_NO_INT128"
cflags="--target=$triple -std=gnu11 -fgnu89-inline -O1 -g0 -w -mlong-double-64 -fno-pic -fno-pie"
nativeld="-no-pie"	# native reference builds link such objects

# parts of libc++ that are not header-only
# (OpenTTD needs iostreams and locale for its console, regex, and
# mutex/condition_variable/thread/chrono, which run single threaded)
libcxxsrcs="string hash charconv algorithm memory variant optional system_error
	filesystem/path filesystem/operations filesystem/directory_iterator filesystem/directory_entry
	filesystem/filesystem_clock filesystem/filesystem_error filesystem/int128_builtins
	ios ios.instantiations iostream locale regex
	mutex mutex_destructor condition_variable condition_variable_destructor thread chrono future
	ryu/d2fixed ryu/d2s ryu/f2s"

fatal()
{
	echo "$(basename "$0"): $*" >&2
	exit 1
}

# put goken's 6c, 6l, rc etc. first in PATH
gokenpath()
{
	[ -x "$goken/ROOT/arch/boot-gcc/bin/6c" ] || fatal "no goken9cc in $goken: run plan9/setup_goken.sh"
	PATH=$goken/bin:$goken/ROOT/arch/boot-gcc/bin:$PATH
	export objtype=amd64
}

# lowereh in.ll out.ll invoke.c: Emscripten-style exception lowering.
# Invokes become calls through __invoke_<sig> wrappers (generated into
# invoke.c) and a test of __THREW__; see eh/eh_rt.h.  The wrappers are
# plain C functions and there are no threads: drop cc99, thread_local.
# Also make member-pointer calls independent of function alignment
# (eh/pmf.py: Plan 9's linker does not align functions).
lowereh()
{
	opt-17 -wasm-lower-em-ehsjlj -enable-emscripten-cxx-exceptions "$1" -S -o - |
		sed 's/ thread_local / /; s/ cc99 / /' >"$2.em" &&
	[ -s "$2.em" ] &&
	python3 "$ehdir/pmf.py" "$2.em" "$2.pmf" >/dev/null &&
	python3 "$ehdir/mkinvoke.py" "$2.pmf" "$2" "$3"
}

# linkcxx out.bc program...: link a C++ program with what it needs from
# the libc++/libc++abi IR archive (build_libcxx.sh), like a static linker
libir=$out/libcxx-ir
linkcxx()
{
	local o=$1
	shift
	[ -f "$libir/stamp" ] || "$ehdir/../build_libcxx.sh" >/dev/null || return 1
	python3 "$ehdir/../arlink.py" "$o" "$@" -- "$libir"/*.bc
}
