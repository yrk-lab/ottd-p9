#!/usr/bin/env python3
# homes units dir
#	Do a static linker's work on the shared symbols of separately
#	translated units: of each symbol several units define as shared
#	("S" in dir/unit.syms, from symtab.py), keep one.  Its home is the
#	unit with an ordinary definition of it, if any (then none keeps a
#	shared copy), else the first unit in the list (units: one unit a
#	line, its name first, in link order) that has it.  Writes
#	dir/unit.home: the shared symbols that unit keeps, sorted;
#	localize.py makes its other shared definitions declarations.  A .home file is written only if
#	it changed: a unit whose list is the same is not translated again.
import os
import sys

if len(sys.argv) != 3:
	sys.exit("usage: homes units dir")
units = [l.split()[0] for l in open(sys.argv[1]) if l.strip()]
d = sys.argv[2]
strong = set()
shared = {}
for u in units:
	for line in open(os.path.join(d, u + ".syms")):
		k, name = line.split()
		if k == "D":
			strong.add(name)
		else:
			shared.setdefault(u, []).append(name)
home = {}
for u in units:
	for name in shared.get(u, []):
		if name not in strong and name not in home:
			home[name] = u
keep = {}
for name, u in home.items():
	keep.setdefault(u, []).append(name)
changed = 0
for u in units:
	new = "".join(n + "\n" for n in sorted(keep.get(u, [])))
	p = os.path.join(d, u + ".home")
	try:
		old = open(p).read()
	except FileNotFoundError:
		old = None
	if new != old:
		open(p, "w").write(new)
		changed += 1
print("homes: %d shared symbols, %d units, %d .home files changed" % (len(home), len(units), changed))
