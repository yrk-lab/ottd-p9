# Phase 1 settings shared by the scripts; sourced, not run.
# These may be set in the environment:
#	work	OpenTTD clone, source trees, builds, game data
#	out	gate runs
#	tag	OpenTTD release

top=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
work=${work:-$top/work}
out=${out:-$top/out}
tag=${tag:-15.3}

git=$work/openttd		# clone of the release tag
ref=$work/src-ref		# stock source, no .git
p9=$work/src-p9			# with patches/series applied
data=$work/data			# XDG_DATA_HOME for test runs: baseset

opengfx=7.1
zlibtag=v1.3.1			# the system's version, so native and translated builds agree
zlib=$work/zlib			# its sources, translated with the program

PATH=/usr/lib/llvm-17/bin:$PATH

fatal()
{
	echo "$(basename "$0"): $*" >&2
	exit 1
}
