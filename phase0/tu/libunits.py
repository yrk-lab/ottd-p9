#!/usr/bin/env python3
# libunits program.bc... -- library.bc...
#	The library modules (libc++ in IR) that separately translated
#	units need, chosen as ../arlink.py chooses them, as a static linker
#	takes archive members: those that define a symbol still undefined,
#	repeatedly.  Prints their paths, one a line, in the order chosen.
#	eh_can_catch is always needed: the exception runtime calls it.
import subprocess
import sys


def symbols(files):
	out = subprocess.run(["llvm-nm-17"] + files, capture_output=True, text=True, check=True).stdout
	defined, undefined, cur = {None: set()}, {None: set()}, None	# one file: no name line
	for line in out.splitlines():
		if line.endswith(":") and " " not in line:
			cur = line[:-1]
			defined[cur], undefined[cur] = set(), set()
			continue
		p = line.split()
		if len(p) < 2:
			continue
		kind, name = p[-2], p[-1]
		if kind == "U":
			undefined[cur].add(name)
		elif kind.isupper() or kind in "vw":
			defined[cur].add(name)
	if len(files) == 1:	# llvm-nm names no file then
		defined = {files[0]: defined.pop(None, set())}
		undefined = {files[0]: undefined.pop(None, set())}
	return defined, undefined


a = sys.argv[1:]
if "--" not in a:
	sys.exit("usage: libunits program.bc... -- library.bc...")
i = a.index("--")
prog, lib = a[:i], a[i+1:]
d, u = symbols(prog)
defined = set().union(*d.values())
undefined = set().union(*u.values()) | {"eh_can_catch"}
ld, lu = symbols(lib)
chosen = []
while True:
	need = undefined - defined
	add = [m for m in lib if m not in chosen and ld[m] & need]
	if not add:
		break
	for m in add:
		chosen.append(m)
		defined |= ld[m]
		undefined |= lu[m]
print("\n".join(chosen))
