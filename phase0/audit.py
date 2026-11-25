#!/usr/bin/env python3
"""Dialect audit of translator output: how far is it from something 6c/8c/pcc could swallow?
Usage: audit.py file.c [...]   Prints counts; these numbers are the Phase 2a/2b gate data."""
import re, sys
PAT = {
 "__attribute__/ATTRIBUTELIST": r"__attribute__|__ATTRIBUTELIST__|__ATTRIBUTE_WEAK__",
 "inline/forceinline/MSVC_INLINE": r"\b__forceinline\b|__MSVC_INLINE__|\binline\b",
 "long long / 128-bit":          r"\blong long\b|__int128|\bllvm_[a-z_]*128",
 "__builtin_*":                  r"__builtin_\w+",
 "designated/compound literal":  r"\{\s*\.\w+\s*=|\(\s*struct [^)]*\)\s*\{",
 "stdint types (C99)":           r"\b(u?int(8|16|32|64)_t)\b",
 "_Static_assert/_Alignas":      r"_Static_assert|_Alignas|__PREFIXALIGN__|__POSTFIXALIGN__",
 "goto":                         r"\bgoto\b",
 "// comments":                  r"//",
}
ident = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
for path in sys.argv[1:]:
    src = open(path, errors="replace").read()
    lines = src.count("\n") + 1
    print(f"{path}: {lines} lines, {len(src)//1024} KiB")
    for k, p in PAT.items():
        print(f"  {k:32s} {len(re.findall(p, src))}")
    ids = set(ident.findall(src))
    lens = sorted((len(i) for i in ids), reverse=True)
    print(f"  identifiers: {len(ids)}; longest {lens[0] if lens else 0}; >28 chars: {sum(l>28 for l in lens)}; >255: {sum(l>255 for l in lens)}")
