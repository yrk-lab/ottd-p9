#!/usr/bin/env python3
# pmf in.ll out.ll
#	Make member-function-pointer calls independent of function alignment.
#	The Itanium C++ ABI marks a virtual member pointer by bit 0 of its
#	pointer word (1 + the vtable offset); a non-virtual one holds the
#	function's address, assumed even.  Plan 9's linker does not align
#	functions, so a member function can have an odd address and would be
#	called as if virtual.  Virtual offsets are small and code lies above
#	the text base (Plan 9 amd64: 0x200000, Linux non-PIE: 0x400000), so
#	the test becomes "bit 0 set and below 0x200000".
#
#	Clang emits, per member-pointer call (X is the pointer word):
#		%A = and i64 %X, 1		-> is it virtual?
#		%B = add i64 %X, -1		the vtable offset
#		%C = getelementptr i8, ptr %vt, i64 %B
#		%D = inttoptr i64 %X to ptr	the function itself
#	Only an "and i64 %X, 1" whose function also has the other three for
#	the same %X is rewritten.
import re
import sys

TEXTBASE = 0x200000

if len(sys.argv) != 3:
	sys.exit("usage: pmf in.ll out.ll")
lines = open(sys.argv[1]).readlines()

andre = re.compile(r"^(\s*)(%[-\w.]+) = and i64 (%[-\w.]+), 1$")
n = 0
start = 0
while start < len(lines):
	if not lines[start].startswith("define "):
		start += 1
		continue
	end = start
	while end < len(lines) and lines[end] != "}\n":
		end += 1
	body = "".join(lines[start:end])
	for i in range(start, end):
		m = andre.match(lines[i].rstrip("\n"))
		if not m:
			continue
		ind, a, x = m.groups()
		xs = re.escape(x)
		add = re.search(r"(%[-\w.]+) = add i64 " + xs + r", -1\b", body)
		if not add:
			continue
		if not re.search(r"= getelementptr (inbounds )?i8, ptr %[-\w.]+, i64 " + re.escape(add.group(1)) + r"\b", body):
			continue
		if not re.search(r"= inttoptr i64 " + xs + r" to ptr\b", body):
			continue
		k = "%%pmf%d" % n
		lines[i] = ("%s%s.lo = icmp ult i64 %s, %d\n" % (ind, k, x, TEXTBASE) +
			"%s%s.bit = and i64 %s, 1\n" % (ind, k, x) +
			"%s%s = select i1 %s.lo, i64 %s.bit, i64 0\n" % (ind, a, k, k))
		n += 1
	start = end + 1
open(sys.argv[2], "w").writelines(lines)
print("pmf: %d member-pointer calls" % n)
