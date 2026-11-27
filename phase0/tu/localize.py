#!/usr/bin/env python3
# localize unit.home < unit.ll > out.ll
#	Settle a unit's shared definitions (linkonce, weak, common: see
#	symtab.py) as homes.py decided: the ones in unit.home become
#	ordinary definitions, the others available_externally, which
#	"opt -passes=elim-avail-extern" then turns into declarations.
#	Comdats go: each symbol now has one definition.  Plan 9's linker
#	has no weak symbols, so each must be defined exactly once.
#	Function aliases (Itanium's C1/D1 constructors and destructors for
#	C2/D2) become copies of their target: llvm-cbe makes an alias a
#	pointer variable, which another unit, calling it as a function,
#	would not find.  (A copy rather than a call: the alias's type does
#	not say which arguments are byval structs.)
#	Inline assembly (only OpenTTD's cpuid) goes too, Plan 9 C has none:
#	an asm call's result is zero, which for cpuid is "no features".
import re
import sys

if len(sys.argv) != 2:
	sys.exit("usage: localize unit.home < unit.ll > out.ll")
keep = set(open(sys.argv[1]).read().split())
shared = r"(linkonce_odr|linkonce|weak_odr|weak|common)"
fn = re.compile(r"^(define\s+)" + shared + r"\s+(.*?@(\"[^\"]+\"|[-\w.$]+)\(.*)$")
gv = re.compile(r"^(@(\"[^\"]+\"|[-\w.$]+)\s*=\s*)" + shared + r"\s+(.*)$")
alias = re.compile(r"^@(\"[^\"]+\"|[-\w.$]+)\s*=\s*(.*?)\balias\s+(.+?)\s+\(([^()]*)\),\s*ptr\s+@(\"[^\"]+\"|[-\w.$]+)\s*$")
comdat = re.compile(r",\s*comdat(\(\$(\"[^\"]+\"|[-\w.$]+)\))?|\s+comdat(\(\$(\"[^\"]+\"|[-\w.$]+)\))?(?=\s|$|\{)")


def name(n):
	return n[1:-1] if n.startswith('"') else n


# @N = [linkage] [attributes] alias R (P...), ptr @T -> a copy of T named N
linkword = r"(?:private|internal|available_externally|linkonce_odr|linkonce|weak_odr|weak|common|external)\s+"


def copyof(m, lines, blocks):
	n, attrs, ret, params, target = m.groups()
	words = attrs.split()
	link = ""
	if any(re.fullmatch(shared, w) for w in words):
		link = "" if name(n) in keep else "available_externally "
	elif "internal" in words or "private" in words:
		link = "internal "
	a, b = blocks[name(target)]
	head = re.sub(r"^define\s+(?:" + linkword + r")?", "define " + link, lines[a], count=1)
	head = head.replace("@" + target + "(", "@" + n + "(", 1)
	return [comdat.sub("", head)] + lines[a+1:b+1]


asm = re.compile(r"^(\s*%[-\w.]+ = )(?:tail |notail )?call (.*?) asm (sideeffect )?.*$")
lines = sys.stdin.readlines()
blocks, start = {}, None
for k, line in enumerate(lines):
	m = re.match(r"^define\s.*?@(\"[^\"]+\"|[-\w.$]+)\(", line)
	if m:
		start, cur = k, name(m.group(1))
	elif line.startswith("}") and start is not None:
		blocks[cur] = (start, k)
		start = None

out = []
for line in lines:
	m = alias.match(line.rstrip("\n"))
	if m and name(m.group(5)) in blocks:
		out += copyof(m, lines, blocks)
		continue
	m = asm.match(line)
	if m:
		t = m.group(2)
		out.append("%sselect i1 true, %s zeroinitializer, %s zeroinitializer\n" % (m.group(1), t, t))
		continue
	if re.match(r"^\s*(tail |notail )?call void asm ", line):
		continue
	if re.match(r"^\$(\"[^\"]+\"|[-\w.$]+)\s*=\s*comdat\b", line):
		continue
	m = fn.match(line)
	if m:
		link = "" if name(m.group(4)) in keep else "available_externally "
		line = m.group(1) + link + comdat.sub("", m.group(3)) + "\n"
	elif gv.match(line):
		m = gv.match(line)
		link = "" if name(m.group(2)) in keep else "available_externally "
		line = m.group(1) + link + comdat.sub("", m.group(4)) + "\n"
	elif line.startswith("define ") or line.startswith("@"):
		line = comdat.sub("", line)	# internal ones in a comdat too
	out.append(line)
sys.stdout.writelines(out)
