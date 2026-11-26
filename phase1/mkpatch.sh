#!/bin/bash
# mkpatch name file...
#	Save the edits made in src-p9 to the given files (paths relative
#	to the source root) as patches/name.patch, a unified diff against
#	src-ref, and add it to patches/series if it is not there yet.
#	One patch per topic keeps the series cheap to rebase on a new tag.
set -euo pipefail
. "$(dirname "$0")/common.sh"

[ $# -ge 2 ] || fatal "usage: mkpatch name file..."
name=$1
shift
p=$top/patches/$name.patch
: >"$p"
for f in "$@"; do
	(cd "$work" && diff -uN "src-ref/$f" "src-p9/$f") >>"$p" || true
done
[ -s "$p" ] || fatal "no changes in $*"
grep -qx "$name.patch" "$top/patches/series" || echo "$name.patch" >>"$top/patches/series"
echo "$p: $(grep -c '^@@' "$p") hunks"
