#!/usr/bin/env python3
"""lower [--libc] [--unit=name,ctors] < cbe.c > plan9.c

Probe-grade stand-in for a cbe 'Plan 9 dialect' mode: llvm-cbe's C on stdin,
C that kencc 6c accepts on stdout.  Everything here is mechanical; the
long-term home for these rules is a dialect switch inside llvm-cbe.
"""
import re, sys
s = sys.stdin.read()
LIBC = "--libc" in sys.argv[1:]	# translated ISO C program: route libc references through libc_p9.c
# --unit=name,ctors: one of separately translated units (../tu); its
# constructors are exported as __p9_ctors_<name>[_<priority>], listed in
# the file ctors, for ../tu/mkctors.py's table
UNIT = [a[7:].split(",", 1) for a in sys.argv[1:] if a.startswith("--unit=")]
# ISO C names whose Plan 9 counterparts are missing or have a different ABI
# (long = 32 bit on Plan 9 amd64); the pthread calls, the program's own
# threads (thread_p9.c), apart from plan9port's use of glibc's
RENAMED = """printf fprintf vprintf vfprintf snprintf vsnprintf putc fputc putchar fputs puts fwrite fflush fopen fclose
getc fgetc getchar fgets perror strtol strtoul atol labs strlen exit stdin stdout stderr main
stat lstat fstat opendir readdir closedir mkdir remove rename getcwd realpath
open read write lseek dup close time strerror isatty
fread fseek fseeko ftell ftello feof ferror clearerr fileno flockfile funlockfile fflush_unlocked ungetc
getwc ungetwc fputwc vasprintf tan
socket bind listen accept connect send sendto recv recvfrom setsockopt getsockopt getsockname getpeername
getaddrinfo freeaddrinfo getnameinfo gai_strerror getifaddrs freeifaddrs select ioctl
pthread_create pthread_join pthread_detach pthread_self pthread_setname_np pthread_key_create
pthread_setspecific pthread_getspecific pthread_mutex_init pthread_mutex_destroy pthread_mutex_lock
pthread_mutex_trylock pthread_mutex_unlock pthread_mutexattr_init pthread_mutexattr_destroy
pthread_mutexattr_settype pthread_cond_destroy pthread_cond_signal pthread_cond_broadcast
pthread_cond_wait pthread_cond_timedwait pthread_cond_clockwait""".split()
# Names Plan 9 libraries define that a program may define too: renamed
# p_<name> (libdraw's globals)
PROGRAM = "display font screen _screen".split()

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
#    Visibility (a unit's hidden symbols, which a whole-program link
#    internalized): Plan 9 has none
s = re.sub(r"\b__HIDDEN__\b", "", s)
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
if not UNIT:
	s += "\nvoid __p9_ctors(void) {\n" + "".join("  %s();\n" % c for c in dict.fromkeys(ctors)) + "}\n"
else:
	#    By priority: 65535 for the ordinary ones.
	byprio = {}
	for c in dict.fromkeys(ctors):
		m = re.match(r"_GLOBAL__I_(\d+)$", c)
		byprio.setdefault(int(m.group(1)) if m else 65535, []).append(c)
	listed = []
	for p in sorted(byprio):
		f = "__p9_ctors_%s%s" % (UNIT[0][0], "" if p == 65535 else "_%d" % p)
		s += "\nvoid %s(void) {\n" % f + "".join("  %s();\n" % c for c in byprio[p]) + "}\n"
		listed.append("%d %s\n" % (p, f))
	open(UNIT[0][1], "w").write("".join(listed))

# 6. --libc: route the RENAMED names to libc_p9.c's c_ versions
if LIBC:
	# rename identifiers only, never inside string literals ("save/autosave/exit.sav")
	ident = re.compile(r"\b(" + "|".join(RENAMED) + r")\b")
	parts = re.split(r'("(?:\\.|[^"\\])*")', s)
	s = "".join(p if i % 2 else ident.sub(r"c_\1", p) for i, p in enumerate(parts))
	# the program's globals that libdraw has too (C++ does not mangle a
	# namespace-scope variable's name: OpenTTD's _screen)
	ident = re.compile(r"\b(" + "|".join(PROGRAM) + r")\b")
	parts = re.split(r'("(?:\\.|[^"\\])*")', s)
	s = "".join(p if i % 2 else ident.sub(r"p_\1", p) for i, p in enumerate(parts))
	# glibc's setjmp is _setjmp: it must be Plan 9's setjmp itself, not a wrapper
	s = re.sub(r"\b_setjmp\b", "setjmp", s)
# 7. --unit: each thread has its own exception state (../eh/eh_rt.h,
#    EH_TLS): __THREW__ is the first field of the thread's, which
#    eh_self gives
threw = ""
if UNIT:
	s = re.sub(r"^extern \w+ __THREW__;\n", "", s, flags=re.M)
	threw = "extern uint64_t *eh_self(void);\n#define __THREW__ (*eh_self())\n"
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
	"#define __builtin_trap() abort()\n" + threw + s)
