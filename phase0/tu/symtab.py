#!/usr/bin/env python3
# symtab unit.bc unit.syms
#	The symbols a unit defines, for homes.py: "D name" for an ordinary
#	(strong) definition, "S name" for one every unit that uses it may
#	have (C++ inline functions and templates, their static locals,
#	vtables, typeinfo: linkonce, weak, common), of which a static linker
#	keeps one.  Sorted, and written only if it changed, so a rebuild
#	that leaves the symbols alone leaves the file's time alone.
import subprocess
import sys

if len(sys.argv) != 3:
	sys.exit("usage: symtab unit.bc unit.syms")
out = subprocess.run(["llvm-nm-17", "--defined-only", sys.argv[1]],
	capture_output=True, text=True, check=True).stdout
lines = set()
for line in out.splitlines():
	p = line.split()
	if len(p) < 2:
		continue
	kind, name = p[-2], p[-1]
	if kind in "WVC":
		lines.add("S " + name)
	elif kind.isupper():
		lines.add("D " + name)
new = "".join(l + "\n" for l in sorted(lines))
try:
	old = open(sys.argv[2]).read()
except FileNotFoundError:
	old = None
if new != old:
	open(sys.argv[2], "w").write(new)
