# Settings shared by the scripts; sourced, not run.
# These may be set in the environment:
#	work	clones and builds (llvm-cbe, libc++ sources, goken9cc)
#	out	per-test output
#	cbe	the llvm-cbe binary
#	goken	goken9cc tree (Plan 9 compilers hosted on Linux)
#	nine	9front root, kernel and u9fs for run_9front.sh
#	triple	clang target

top=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
work=${work:-$top/work}
out=${out:-$top/out}
cbe=${cbe:-$work/llvm-cbe/build/tools/llvm-cbe/llvm-cbe}
goken=${goken:-$work/goken9cc}
nine=${nine:-$HOME/9front}
triple=${triple:-x86_64-unknown-linux-gnu}	# Phase 2+: a Plan 9 triple

PATH=/usr/lib/llvm-17/bin:$PATH

# The language mode the OpenTTD build will use.  -fno-exceptions makes
# try/catch a hard error: cbe's LowerInvokePass would otherwise drop
# the handlers silently.
cxxflags="--target=$triple -std=c++20 -stdlib=libc++ -fno-exceptions -fno-rtti -O1 -g0 -D_LIBCPP_DISABLE_EXTERN_TEMPLATE"
cflags="--target=$triple -std=gnu11 -fgnu89-inline -O1 -g0 -w"

# parts of libc++ that are not header-only
libcxxsrcs="string hash charconv algorithm memory ryu/d2fixed ryu/d2s ryu/f2s"

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
