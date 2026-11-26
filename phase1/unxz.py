#!/usr/bin/env python3
# unxz in.sav out.sav
#	Rewrite an OpenTTD save compressed with liblzma ("OTTX": tag, 4-byte
#	version, an xz stream) as an uncompressed one ("OTTN": tag, the same
#	version, the raw data), for builds without liblzma.
import lzma
import sys

if len(sys.argv) != 3:
	sys.exit("usage: unxz in.sav out.sav")
d = open(sys.argv[1], "rb").read()
if d[:4] == b"OTTN":
	open(sys.argv[2], "wb").write(d)
	sys.exit(0)
if d[:4] != b"OTTX":
	sys.exit("unxz: %s: not an xz save (%r)" % (sys.argv[1], d[:4]))
open(sys.argv[2], "wb").write(b"OTTN" + d[4:8] + lzma.decompress(d[8:]))
