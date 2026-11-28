#!/bin/bash
# ft9
#	FreeType's public headers in Plan 9's data model, for the
#	translation: work/ft9/include.  On 9front FreeType is built
#	natively by its own mkfile (6c, builds/plan9), where long is 32
#	bits, as are size_t and ptrdiff_t (its p9lib.h makes them ulong
#	and int); clang's x86-64 long is 64.  LLVM IR has no long, so it
#	matters only where the translated code meets FreeType: its
#	structs and calls.  These headers say what 6c means: every long
#	that is not part of long long is int (FT_Long, FT_Pos, FT_Fixed,
#	the structs' unsigned longs), size_t unsigned int, FT_PtrDist int,
#	FT_SIZEOF_LONG 4; the options are those of the native build
#	(p9ftopt.h: no zlib, no LZW).  Pointers, long long and enums are
#	the same on both sides.  layout9.c checks the structs OpenTTD
#	reads against 6c's.
set -euo pipefail
. "$(dirname "$0")/common.sh"
[ -d "$freetype/include" ] || fatal "no $freetype: run setup.sh"
# made in ft9.new, kept only if it differs (the build's dependencies
# are the headers' times)
final=$work/ft9
o=$work/ft9.new
rm -rf "$o"
mkdir -p "$o"
cp -r "$freetype/include" "$o/include"
find "$o/include" -name '*.h' -print0 | xargs -0 sed -i -E '
	s/long long/LONG_LONG_/g
	s/\blong\b/int/g
	s/LONG_LONG_/long long/g
	s/\bsize_t\b/unsigned int/g
	s/\bULONG_MAX\b/UINT_MAX/g
	s/\bLONG_MAX\b/INT_MAX/g
	s/\bLONG_MIN\b/INT_MIN/g
	s/(#define ft_ptrdiff_t +)ptrdiff_t/\1int/
'
# 6c rounds every struct up to a multiple of 8 bytes, clang does not:
# where that makes a size differ (layout9.c), an explicit pad
sed -i '/typedef struct  FT_Size_Metrics_/,/} FT_Size_Metrics;/ s/^\( *FT_Pos *max_advance;.*\)$/\1\n    FT_Int     pad6c_;      \/* 6c: 32 bytes *\//' \
	"$o/include/freetype/freetype.h"
grep -q pad6c_ "$o/include/freetype/freetype.h" || fatal "freetype.h: FT_Size_Metrics not padded"
# integer-types.h would take long's size from the host's limits
sed -i '0,/#ifndef FT_SIZEOF_LONG$/s//#define FT_SIZEOF_LONG  ( 32 \/ FT_CHAR_BIT )\n&/' \
	"$o/include/freetype/config/integer-types.h"
# the native build's options (its mkfile's p9ftopt.h)
sed -i '
	/#define FT_CONFIG_OPTION_USE_LZW/ s!^!// !
	/#define FT_CONFIG_OPTION_USE_ZLIB/ s!^!// !
' "$o/include/freetype/config/ftoption.h"
grep -q '#define FT_SIZEOF_LONG  ( 32' "$o/include/freetype/config/integer-types.h" ||
	fatal "integer-types.h: FT_SIZEOF_LONG not set"
if [ -d "$final" ] && diff -rq "$o" "$final" >/dev/null; then
	rm -rf "$o"
else
	rm -rf "$final"
	mv "$o" "$final"
fi
echo "$final/include"
