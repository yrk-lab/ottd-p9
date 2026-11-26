#!/usr/bin/env python3
"""Probe-grade stand-in for a cbe 'Plan 9 dialect' mode: cbe C on stdin -> C that kencc 6c accepts, on stdout.
Everything here is mechanical; the long-term home for these rules is a dialect switch inside llvm-cbe."""
import re, sys
s = sys.stdin.read()

# 1. Preamble: 6c has no #if. Drop every #if..#endif block and all #includes; keep helper function definitions.
m = re.search(r"/\* Provide Declarations \*/(.*?)/\* Global Declarations \*/", s, flags=re.S)
depth, kept = 0, []
for line in m.group(1).split("\n"):
    t = line.strip()
    if re.match(r"#\s*if", t): depth += 1; continue
    if re.match(r"#\s*endif", t): depth -= 1; continue
    if depth == 0 and not re.match(r"#\s*include", t): kept.append(line)
s = s[:m.start()] + "/* Provide Declarations */\n" + "\n".join(kept) + "\n/* Global Declarations */" + s[m.end():]

# 2. GNU attribute macros with balanced parentheses
def strip_call(s, name):
    out, i = [], 0
    pat = re.compile(r"\b" + name + r"\s*\(")
    while True:
        m = pat.search(s, i)
        if not m: out.append(s[i:]); break
        out.append(s[i:m.start()]); j, d = m.end(), 1
        while d: d += {'(': 1, ')': -1}.get(s[j], 0); j += 1
        i = j
    return "".join(out)
for n in ("__ATTRIBUTELIST__", "__attribute__", "__POSTFIXALIGN__", "__PREFIXALIGN__", "__FUNCTIONALIGN__"):
    s = strip_call(s, n)
s = re.sub(r"__ATTRIBUTE_WEAK__|\b__forceinline\b|\b__noreturn\b", "", s)
s = re.sub(r"\bstatic\s+inline\b", "static", s)

# 3. 6c rejects 'typedef R f(args);' -> pointer-to-function typedefs
s = re.sub(r"typedef\s+([^;(]*?)\s+(l_fptr_\d+)\s*\(([^;]*)\);", r"typedef \1 (*\2)(\3);", s)
s = re.sub(r"\b(l_fptr_\d+)\s*\*", r"\1 ", s)

# 4. 64-bit literals: no ## in 6c's preprocessor, so emit suffixed literals directly
s = re.sub(r"\bUINT64_C\(\s*(\d+)\s*\)", r"\1ull", s)
s = re.sub(r"\bINT64_C\(\s*(-?\d+)\s*\)", lambda m: m.group(1) + "ll", s)

sys.stdout.write("#include <stdint.h>\n#include <math.h>\ntypedef unsigned char bool;\n"
                 "#define __UNALIGNED_LOAD__(type, align, op) (*(type*)(op))\n"
                 "void abort(void);\n#define __builtin_unreachable() abort()\n#define __builtin_trap() abort()\n" + s)
