#!/usr/bin/env python3
# mkinvoke lowered.ll fixed.ll invoke.c
#	Write the C wrappers that the Emscripten exception lowering left
#	declared in an LLVM IR module, and a copy of the module (fixed.ll)
#	in which every call of a variadic wrapper (an invoke of a variadic
#	function) uses its own non-variadic wrapper, named after the types
#	of its extra arguments: C cannot forward "...", but it can call a
#	variadic function with concrete arguments.  fixed.ll also has no
#	sret attributes: llvm-cbe turns an sret function into one that
#	returns the struct by value, which a wrapper calling it with the
#	IR's (ptr, args...) signature would not match.  Without sret the
#	result pointer is an ordinary first parameter, in the definition
#	and at every call.  The wrappers are:
#	__invoke_<sig>(fn, args...)	call fn(args...) under a setjmp frame;
#				if it throws, set __THREW__ and return 0
#	__cxa_find_matching_catch_<n>	collect the n-2 catch types for eh_match
#	Names are mangled as llvm-cbe does (CBEMangle), and literal struct
#	types ({ ptr, i8 }) get a C struct with the LLVM layout: explicit
#	padding, packed, as cbe -cbe-explicit-padding lays out its own.
#	See eh_rt.h.
import re
import sys

scalars = {	# IR type: C type, size and alignment on x86-64
	"i1": ("unsigned char", 1),
	"i8": ("unsigned char", 1),
	"i16": ("unsigned short", 2),
	"i32": ("unsigned int", 4),
	"i64": ("unsigned long long", 8),
	"ptr": ("void*", 8),
	"float": ("float", 4),
	"double": ("double", 8),
}
structs = {}	# IR literal struct type: (C name, size, align)
decls = []	# C struct definitions, in order

def mangle(name):
	r = ""
	for c in name:
		if c.isalnum() or c == "_":
			r += c
		else:
			r += "_" + chr(ord("A") + (ord(c) & 15)) + chr(ord("A") + ((ord(c) >> 4) & 15)) + "_"
	return r

def clean(t):
	return re.sub(r"\s+(noundef|nonnull|signext|zeroext|noalias|nocapture|readonly|writeonly|"
		r"returned|dereferenceable(_or_null)?\(\d+\)|align \d+)\b", "", " " + t).strip()

# ctype(t) -> (C type, size, align)
def ctype(t):
	t = clean(t)
	if t == "void":
		return ("void", 0, 1)
	if t in scalars:
		c, n = scalars[t]
		return (c, n, n)
	if t.startswith("{") and t.endswith("}"):
		return struct(t)
	sys.exit("mkinvoke: unsupported IR type in a wrapper: %r" % t)

def struct(t):
	if t in structs:
		return structs[t]
	fields = [ctype(f) for f in split(t[1:-1])]
	name = "struct eh_s%d" % len(structs)
	body, off, align = [], 0, 1
	for i, (c, n, a) in enumerate(fields):
		pad = -off % a
		if pad:
			body.append("\tunsigned char\tpad%d[%d];\n" % (i, pad))
			off += pad
		body.append("\t%s\tf%d;\n" % (c, i))
		off += n
		align = max(align, a)
	if -off % align:
		body.append("\tunsigned char\tpadtail[%d];\n" % (-off % align))
		off += -off % align
	decls.append("%s\n{\n%s};\n" % (name, "".join(body)))
	structs[t] = (name, off, align)
	return structs[t]

def split(params):
	ps, depth, cur = [], 0, ""
	for c in params:
		if c in "({[<":
			depth += 1
		elif c in ")}]>":
			depth -= 1
		if c == "," and depth == 0:
			ps.append(cur)
			cur = ""
		else:
			cur += c
	if cur.strip():
		ps.append(cur)
	return [p.strip() for p in ps]

# nfixed: for a specialized variadic wrapper, the number of the
# callee's fixed parameters; the rest are passed through "..."
def invoke(name, ret, params, nfixed=None):
	r = ctype(ret)[0]
	args = [ctype(p)[0] for p in params[1:]]	# params[0] is the function
	formal = ", ".join(["void *fn"] + ["%s a%d" % (t, i) for i, t in enumerate(args)])
	actual = ", ".join("a%d" % i for i in range(len(args)))
	if nfixed is None:
		fntype = "%s (*)(%s)" % (r, ", ".join(args) or "void")
	else:
		fntype = "%s (*)(%s)" % (r, ", ".join(args[:nfixed] + ["..."]))
	out = ["%s\n%s(%s)\n{\n\tEhframe f;\n" % (r, name, formal)]
	if r != "void":
		out.append("\t%s r;\n\tstatic %s zero;\n" % (r, r))
	out.append("\n\tf.prev = eh_top;\n\teh_top = &f;\n\tif(setjmp(f.jb) == 0){\n")
	if r == "void":
		out.append("\t\t((%s)fn)(%s);\n" % (fntype, actual))
	else:
		out.append("\t\tr = ((%s)fn)(%s);\n" % (fntype, actual))
	out.append("\t\teh_top = f.prev;\n")
	out.append("\t\treturn%s;\n\t}\n" % ("" if r == "void" else " r"))
	out.append("\teh_top = f.prev;\n\t__THREW__ = 1;\n")
	if r != "void":
		out.append("\treturn zero;\n")
	out.append("}\n\n")
	return "".join(out)

def findcatch(name, n):
	k = n - 2
	formal = ", ".join("void *t%d" % i for i in range(k)) or "void"
	if k == 0:
		body = "\treturn eh_match(0, 0);\n"
	else:
		body = "\tvoid *t[%d];\n\n" % k
		body += "".join("\tt[%d] = t%d;\n" % (i, i) for i in range(k))
		body += "\treturn eh_match(t, %d);\n" % k
	return "void*\n%s(%s)\n{\n%s}\n\n" % (name, formal, body)

# the type of one call argument: "ptr noundef nonnull %3" -> "ptr"
def argtype(a):
	a = a.strip()
	if a.startswith("{"):
		depth = 0
		for i, c in enumerate(a):
			depth += c == "{"
			depth -= c == "}"
			if depth == 0:
				return a[:i+1]
	return a.split()[0]

def typename(t):
	return re.sub(r"[^a-z0-9]+", "", clean(t))

if len(sys.argv) != 4:
	sys.exit("usage: mkinvoke lowered.ll fixed.ll invoke.c")
lines = open(sys.argv[1]).readlines()

# drop sret(<type>) attributes
sretre = re.compile(r'\s*\bsret\((%"[^"]*"|%[-\w.]+|\{[^}]*\}|\[[^\]]*\]|[\w.]+)\)')
lines = [sretre.sub("", l) for l in lines]

# specialize the calls of variadic wrappers
variadic = {}	# specialized name: (ret, fixed params, extra types)
callre = re.compile(r'call (\S.*?) \((.*), \.\.\.\) @"?(__invoke_[^"(]*\.\.\.)"?\((.*)\)(.*)$')
for i, line in enumerate(lines):
	m = callre.search(line)
	if not m:
		continue
	ret, fixed, name, args, rest = m.groups()
	fixed = split(fixed)
	args = split(args)
	extra = [argtype(a) for a in args[len(fixed):]]
	spec = name.replace("...", "v") + "".join("_" + typename(t) for t in extra)
	variadic[spec] = (ret, fixed, extra)
	lines[i] = line[:m.start()] + 'call %s @"%s"(%s)%s\n' % (ret, spec, ", ".join(args), rest.rstrip("\n"))
lines = [l for l in lines if not re.match(r'declare .*@"?__invoke_[^"(]*\.\.\."?\(', l)]
for spec, (ret, fixed, extra) in variadic.items():
	lines.append('declare %s @"%s"(%s)\n' % (ret, spec, ", ".join(fixed + extra)))
open(sys.argv[2], "w").writelines(lines)

funcs = []
seen = set()
for line in lines:
	m = re.match(r'declare\s+(?:[a-z_]+\s+)*?(\S.*?)\s+@"?(__invoke_[^"(]+|__cxa_find_matching_catch_(\d+))"?\((.*)\)', line)
	if not m or m.group(2) in seen:
		continue
	seen.add(m.group(2))
	if m.group(3):
		funcs.append(findcatch(m.group(2), int(m.group(3))))
	elif m.group(2) in variadic:
		ret, fixed, extra = variadic[m.group(2)]
		funcs.append(invoke(mangle(m.group(2)), ret, fixed + extra, len(fixed) - 1))
	else:
		funcs.append(invoke(mangle(m.group(2)), m.group(1), split(m.group(4))))

out = ["/* generated by mkinvoke.py: do not edit */\n#include \"eh_sys.h\"\n#include \"eh_rt.h\"\n\n"]
if decls:
	out.append("#ifdef __GNUC__\n#pragma pack(push, 1)\n#else\n#pragma pack on\n#endif\n")
	out.append("\n".join(decls))
	out.append("#ifdef __GNUC__\n#pragma pack(pop)\n#else\n#pragma pack off\n#endif\n\n")
open(sys.argv[3], "w").write("".join(out + funcs))
