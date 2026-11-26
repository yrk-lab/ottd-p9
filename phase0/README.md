# Phase 0: feasibility harness (C++20 -> LLVM IR -> llvm-cbe -> C)

Question answered: *can the clang -> llvm-cbe pipeline translate C++20 code that uses classes, templates, lambdas, virtual
dispatch, static init and the STL (with libc++ itself translated too), and produce programs whose behaviour is identical
to the native build?*

## Run it (WSL2 Ubuntu 24.04)
```
./setup.sh          # apt packages, pinned+patched llvm-cbe, libc++ 17.0.6 sources (~12 MB sparse clone)
./run_phase0.sh     # builds libc++ IR once, then runs every test through the pipeline
./run_phase0.sh t3_stl
```
Layout: `tests/` positive tests, `tests_negative/` must-fail test, `rt.c` tiny runtime shim (new/delete, pure-virtual, atomics,
std::exception stubs), `patches/` fixes to llvm-cbe, `audit.py` dialect/identifier statistics, `out/` per-test logs and artifacts.

## Result on my run (sandbox, Ubuntu 24.04, clang/LLVM 17.0.6, gcc as stand-in C compiler, x86_64)
| test | content | result | C lines |
|---|---|---|---|
| t1_c_like | structs, integer hashing, printf | PASS | 126 |
| t2_classes | virtuals, ctors/dtors, namespaces, templates, lambdas, dynamic static init, RAII | PASS | 325 |
| t3_stl | vector, string, unordered_map, map, sort, structured bindings, optional, variant, span, unique_ptr, to_string | PASS | 11,040 |
| neg_exceptions | try/catch with `-fno-exceptions` | rejected at clang (as intended) | - |

**GATE 0: PASS.** Output byte-identical to the native clang/libc++ build for all three.

## What it took (three llvm-cbe fixes, in `patches/llvm-cbe-phase0.patch`)
1. `llvm.fshl/fshr` (rotates, used by libc++'s string hash) were unsupported: added.
2. Itanium constructor/destructor *aliases* (C1 -> C2) crashed cbe (alignment query on a function type): guarded.
3. Early forward declarations of internal functions were emitted without `static`, which gcc rejects: fixed.
Also: llvm-cbe master now needs LLVM 22; setup pins rev 5341f88 (last before LLVM 19 support), which works with LLVM 17.

## Data for the later phases (from `audit.py`, t3_stl)
- GNU-isms are everywhere: `__attribute__`, `__forceinline`, `__builtin_*`, C99 stdint types (3.7k uses), alignment macros.
  => Phase 2 needs either an old gcc under APE or a dialect-lowering patch to cbe; 6c would reject this output today.
- Identifiers up to 398 characters, 75 over 28, 6 over 255 (mangled template names). => symbol-hashing pass is required for Plan 9 linkers.
- Bloat: a ~40-line program becomes 11k lines / 347 KB of C even after pruning, because libc++ templates are instantiated into it.
  For OpenTTD, link all TUs into one module and run globaldce *before* cbe, so each instantiation appears once.

## Not covered by Phase 0 (do not read more into the PASS)
- Exceptions (disabled; OpenTTD has a few try/catch sites to rewrite), threads, `std::filesystem`, fmt, floating-point printing paths,
  32-bit targets, optimisation levels other than -O1, and scale (OpenTTD is ~hundreds of thousands of lines).
- gcc stands in for the Plan 9 C compiler: nothing here shows 6c/8c/pcc or Plan 9 libc accepting the output.
- The runtime shim uses Linux libc; Plan 9/APE equivalents come in Phase 2.

## Phase 0.5 (Plan 9 compiler acceptance)
```
plan9/setup_goken.sh      # builds goken9cc (kencc hosted on Linux) + Plan 9-style libc.a  (~10 min)
plan9/run_p9.sh           # all tests through 6c; output must equal the native build
plan9/p9bundle.sh         # lowered C + shims for a run on real 9front: out/p9bundle/run9front.rc (untested)
```
Result in my sandbox: 8/8 identical (t1-t3, m1-m5). See PLAN.md for what 6c needed and the cbe patch (`-cbe-explicit-padding`).
