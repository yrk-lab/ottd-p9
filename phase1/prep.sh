#!/bin/bash
# prep
#	Make src-p9: a fresh copy of src-ref with the patches listed in
#	patches/series applied in order.  Any edits in src-p9 are lost;
#	save them first with mkpatch.sh.
set -euo pipefail
. "$(dirname "$0")/common.sh"

[ -d "$ref" ] || fatal "no $ref: run setup.sh"
rm -rf "$p9"
cp -a "$ref" "$p9"
n=0
if [ -f "$top/patches/series" ]; then
	while read -r p; do
		case $p in
		''|'#'*)	continue ;;
		esac
		patch -s -d "$p9" -p1 <"$top/patches/$p" || fatal "patch $p failed"
		n=$((n+1))
	done <"$top/patches/series"
fi
echo "$p9: $n patches"
