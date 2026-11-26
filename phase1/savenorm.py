#!/usr/bin/env python3
# savenorm in.sav out.sav
#	Copy an uncompressed OpenTTD save, blanking the fields that are
#	not game state, so that saves of the same game compare equal:
#	DATE.id, the random session id (_game_session_stats.savegame_id,
#	for the opt-in survey): 32 hex digits after its length byte.
import re
import sys

if len(sys.argv) != 3:
	sys.exit("usage: savenorm in.sav out.sav")
d = bytearray(open(sys.argv[1], "rb").read())
if d[:4] != b"OTTN":
	sys.exit("savenorm: %s: not an uncompressed save" % sys.argv[1])

date = d.find(b"DATE")
view = d.find(b"VIEW", date)
if date < 0 or view < 0:
	sys.exit("savenorm: %s: no DATE chunk" % sys.argv[1])
m = list(re.finditer(rb"\x20[0-9A-F]{32}", d[date:view]))
if len(m) != 1:
	sys.exit("savenorm: %s: savegame id not found" % sys.argv[1])
s = date + m[0].start() + 1
d[s:s+32] = b"0" * 32
open(sys.argv[2], "wb").write(d)
