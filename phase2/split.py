#!/usr/bin/env python3
# split prog.c outdir [chunk-MB]
#	Split one llvm-cbe C file (after lower.py) into units a Plan 9 C
#	compiler can take one at a time (kencc keeps a whole unit in memory):
#	outdir/prog.h	everything before the global definitions, with the
#			globals declared extern and the functions declared
#			non-static, and the static intrinsic helpers (each unit
#			gets its own copy)
#	outdir/data.c	the global variable definitions
#	outdir/fnNNN.c	the function bodies, about chunk-MB (default 4) each
#	LLVM has made every module-level name unique and C++ file-local names
#	are mangled, so the static names can become global ones.
import os
import re
import sys

def marker(lines, text):
	for i, l in enumerate(lines):
		if l.startswith("/* " + text + " */"):
			return i
	sys.exit("split: no section '%s'" % text)

def unstatic(l):
	# "static T x", "const static T x", "static const T x" at the start of a line
	l = re.sub(r"^const static ", "const ", l)
	return re.sub(r"^static ", "", l)

def main():
	if len(sys.argv) < 3:
		sys.exit("usage: split prog.c outdir [chunk-MB]")
	src, outdir = sys.argv[1], sys.argv[2]
	chunk = float(sys.argv[3] if len(sys.argv) > 3 else 4) * 1e6
	lines = open(src).readlines()

	gvdecl = marker(lines, "Global Variable Declarations")
	fdecl = marker(lines, "Function Declarations")
	gvdef = marker(lines, "Global Variable Definitions and Initialization")
	alias = marker(lines, "External Alias Declarations")
	intr = marker(lines, "LLVM Intrinsic Builtin Function Bodies")
	bodies = marker(lines, "Function Bodies")
	os.makedirs(outdir, exist_ok=True)
	name = os.path.splitext(os.path.basename(src))[0]

	# prototypes appear before the declaration sections too (next to the
	# types that need them): every "static ...);" prototype becomes global
	h = [unstatic(l) if l.startswith("static ") and l.rstrip().endswith(");") else l for l in lines[:gvdecl]]
	for l in lines[gvdecl:fdecl]:
		if l.startswith("static ") or l.startswith("const static "):
			l = "extern " + unstatic(l)
		h.append(l)
	h += [unstatic(l) for l in lines[fdecl:gvdef]]
	h += lines[alias:bodies]	# aliases (none so far) and the helpers, static
	open(os.path.join(outdir, name + ".h"), "w").writelines(h)

	inc = '#include "%s.h"\n\n' % name
	open(os.path.join(outdir, "data.c"), "w").writelines([inc] + [unstatic(l) for l in lines[gvdef:alias]])

	# function bodies: a definition starts at column 0 and ends with "}" at column 0
	n, size, out, nf = 0, 0, [], 0
	def flush():
		nonlocal n, size, out
		if out:
			open(os.path.join(outdir, "fn%03d.c" % n), "w").writelines([inc] + out)
			n += 1
		size, out = 0, []
	infn = False
	for l in lines[bodies + 1:]:
		if not infn and l[:1] not in ("", " ", "\t", "\n", "}", "#", "/") and l.rstrip().endswith("{"):
			infn = True
			nf += 1
			l = unstatic(l)
		out.append(l)
		size += len(l)
		if infn and l.startswith("}"):
			infn = False
			if size >= chunk:
				flush()
	flush()
	print("split: %s.h %d lines, data.c, %d function units, %d functions" % (name, len(h), n, nf))

main()
