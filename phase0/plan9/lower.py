#!/usr/bin/env python3
"""lower [--libc] < cbe.c > plan9.c

Probe-grade stand-in for a cbe 'Plan 9 dialect' mode: llvm-cbe's C on stdin,
C that kencc 6c accepts on stdout.  Everything here is mechanical; the
long-term home for these rules is a dialect switch inside llvm-cbe.
"""
import re, sys
s = sys.stdin.read()
LIBC = "--libc" in sys.argv[1:]	# translated ISO C program: route libc references through libc_p9.c
# ISO C names whose Plan 9 counterparts are missing or have a different ABI
# (long = 32 bit on Plan 9 amd64)
RENAMED = """printf fprintf vprintf vfprintf snprintf vsnprintf putc fputc putchar fputs puts fwrite fflush fopen fclose
getc fgetc getchar fgets perror strtol strtoul atol labs strlen exit stdin stdout stderr main
stat lstat fstat opendir readdir closedir mkdir remove rename getcwd realpath
open read write lseek dup time strerror isatty
fread fseek fseeko ftell ftello feof ferror clearerr fileno flockfile funlockfile fflush_unlocked ungetc
getwc ungetwc fputwc vasprintf tan
bind listen accept""".split()

# 1. Preamble: 6c has no #if.  Drop every #if..#endif block and all
#    #includes; keep the helper function definitions.
m = re.search(r"/\* Provide Declarations \*/(.*?)/\* Global Declarations \*/", s, flags=re.S)
depth, kept = 0, []
for line in m.group(1).split("\n"):
	t = line.strip()
	if re.match(r"#\s*if", t):
		depth += 1
		continue
	if re.match(r"#\s*endif", t):
		depth -= 1
		continue
	if depth == 0 and not re.match(r"#\s*include", t):
		kept.append(line)
s = (s[:m.start()] + "/* Provide Declarations */\n" + "\n".join(kept) +
	"\n/* Global Declarations */" + s[m.end():])

# 2. GNU attribute macros, with balanced parentheses
def strip_call(s, name):
	out, i = [], 0
	pat = re.compile(r"\b" + name + r"\s*\(")
	while True:
		m = pat.search(s, i)
		if not m:
			out.append(s[i:])
			break
		out.append(s[i:m.start()])
		j, d = m.end(), 1
		while d:
			d += {'(': 1, ')': -1}.get(s[j], 0)
			j += 1
		i = j
	return "".join(out)
for n in ("__ATTRIBUTELIST__", "__attribute__", "__POSTFIXALIGN__", "__PREFIXALIGN__", "__FUNCTIONALIGN__"):
	s = strip_call(s, n)
s = re.sub(r"__ATTRIBUTE_WEAK__|\b__forceinline\b|\b__noreturn\b", "", s)
s = re.sub(r"\bstatic\s+inline\b", "static", s)

# 3. 6c rejects 'typedef R f(args);': pointer-to-function typedefs instead
s = re.sub(r"typedef\s+([^;(]*?)\s+(l_fptr_\d+)\s*\(([^;]*)\);", r"typedef \1 (*\2)(\3);", s)
s = re.sub(r"\b(l_fptr_\d+)\s*\*", r"\1 ", s)

# 4. 64-bit literals: there is no ## in 6c's preprocessor, so emit
#    suffixed literals directly
s = re.sub(r"\bUINT64_C\(\s*(\d+)\s*\)", r"\1ull", s)
s = re.sub(r"\bINT64_C\(\s*(-?\d+)\s*\)", lambda m: m.group(1) + "ll", s)

# 5. Static constructors: kencc has no __attribute__((constructor)).
#    Collect them in the order cbe declares them (module order) and call
#    them from __p9_ctors, which libc_p9.c's main runs before the
#    program's.  Static destructors are not run (as with rt_p9's
#    __cxa_atexit).
ctors = re.findall(r"\b(\w+)\(void\)[^;\n]*__ATTRIBUTE_CTOR__\s*;", s)
#    Constructors with an init_priority (clang names them _GLOBAL__I_<priority>,
#    libc++'s iostreams use 100) run first, lowest priority first.
prio = sorted((c for c in ctors if re.match(r"_GLOBAL__I_\d+$", c)), key=lambda c: int(c[11:]))
ctors = prio + [c for c in ctors if c not in prio]
s = re.sub(r"__ATTRIBUTE_(CTOR|DTOR)__", "", s)
s += "\nvoid __p9_ctors(void) {\n" + "".join("  %s();\n" % c for c in dict.fromkeys(ctors)) + "}\n"

# 6. --libc: route the RENAMED names to libc_p9.c's c_ versions
if LIBC:
	# rename identifiers only, never inside string literals ("save/autosave/exit.sav")
	ident = re.compile(r"\b(" + "|".join(RENAMED) + r")\b")
	parts = re.split(r'("(?:\\.|[^"\\])*")', s)
	s = "".join(p if i % 2 else ident.sub(r"c_\1", p) for i, p in enumerate(parts))
	# glibc's setjmp is _setjmp: it must be Plan 9's setjmp itself, not a wrapper
	s = re.sub(r"\b_setjmp\b", "setjmp", s)
sys.stdout.write(
	"#include <stdint.h>\n"
	"#include <limits.h>\n"
	"#include <math.h>\n"
	"#include <stdarg.h>\n"
	"#include <setjmp.h>\n"
	"typedef unsigned char bool;\n"
	"#define __UNALIGNED_LOAD__(type, align, op) (*(type*)(op))\n"
	"void abort(void);\n"
	"#define __builtin_unreachable() for(;;) abort()\n"
	"#define __builtin_trap() abort()\n" + s)
