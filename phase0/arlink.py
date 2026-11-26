#!/usr/bin/env python3
# arlink out.bc program.{ll,bc}... -- library.bc...
#	Link LLVM IR like a static linker links an archive: all program
#	files, and of the library modules only those that define a symbol
#	still undefined, repeatedly, until nothing more is needed.  A whole
#	module comes in or stays out, with its static constructors: linking
#	all of libc++ into one module would keep iostreams and locale alive
#	in every program (their constructors are always linked).
#	eh_can_catch always counts as needed: the exception runtime
#	(eh/eh_rt.c, outside the IR, linked into every C++ program) calls it.
import os
import subprocess
import sys
import tempfile

def symbols(f):
	out = subprocess.run(["llvm-nm", f], capture_output=True, text=True, check=True).stdout
	defined, undefined = set(), set()
	for line in out.splitlines():
		p = line.split()
		if len(p) < 2:
			continue
		kind, name = p[-2], p[-1]
		if kind == "U":
			undefined.add(name)
		elif kind.isupper() or kind in "vw":	# global (W, V: weak; v, w: weak undefined-ok)
			defined.add(name)
	return defined, undefined

def main():
	a = sys.argv[1:]
	if "--" not in a or len(a) < 4:
		sys.exit("usage: arlink out.bc program... -- library.bc...")
	out = a[0]
	i = a.index("--")
	prog, lib = a[1:i], a[i+1:]

	with tempfile.TemporaryDirectory() as t:
		# the program's own symbols
		first = os.path.join(t, "prog.bc")
		subprocess.run(["llvm-link", "-o", first] + prog, check=True)
		defined, undefined = symbols(first)
		undefined.add("eh_can_catch")
		mods = {m: symbols(m) for m in lib}
		chosen = []
		while True:
			need = undefined - defined
			add = [m for m in lib if m not in chosen and mods[m][0] & need]
			if not add:
				break
			for m in add:
				chosen.append(m)
				defined |= mods[m][0]
				undefined |= mods[m][1]
		subprocess.run(["llvm-link", "-o", out, first] + chosen, check=True)
	print("arlink: %d of %d library modules: %s" % (len(chosen), len(lib),
		" ".join(os.path.basename(m).rsplit(".", 1)[0] for m in chosen)))

main()
