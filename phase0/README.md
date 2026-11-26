# Phase 0: feasibility harness (C++20 -> LLVM IR -> llvm-cbe -> C)

Question answered: *can the clang -> llvm-cbe pipeline translate C++20 code that uses classes, templates, lambdas, virtual
dispatch, static init and the STL (with libc++ itself translated too), and produce programs whose behaviour is identical
to the native build?*

## Run it (WSL2 Ubuntu 24.04 or 26.04)
```
./setup.sh          # apt packages, pinned+patched llvm-cbe, libc++ 17.0.6 sources (~12 MB sparse clone)
./run_phase0.sh     # builds libc++ IR once, then runs every test through the pipeline
./run_phase0.sh t3_stl
```
Layout: `tests/` tests (t1-t3 language/STL, e1-e5 exceptions), `rt.c` tiny runtime shim (new/delete, pure-virtual, atomics),
`eh/` the exception runtime, `patches/` fixes to llvm-cbe, `audit.py` dialect/identifier statistics, `out/` per-test logs and artifacts.
Since 2026-11-26 every C++ test is built with exceptions and RTTI (see the section on exceptions below); the results
table just below is the original -fno-exceptions run.

## Result on my run (sandbox, Ubuntu 24.04, clang/LLVM 17.0.6, gcc as stand-in C compiler, x86_64)
| test | content | result | C lines |
|---|---|---|---|
| t1_c_like | structs, integer hashing, printf | PASS | 126 |
| t2_classes | virtuals, ctors/dtors, namespaces, templates, lambdas, dynamic static init, RAII | PASS | 325 |
| t3_stl | vector, string, unordered_map, map, sort, structured bindings, optional, variant, span, unique_ptr, to_string | PASS | 11,040 |

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
plan9/setup_goken.sh      # once: goken9cc (kencc hosted on Linux) + Plan 9 style libc.a  (~10 min)
plan9/run_p9.sh           # C++ tests through 6c (fast loop); output must equal the native build
plan9/run_compcert.sh     # goken's 24 CompCert benchmarks (ISO C) the same way
plan9/p9check.sh f.cpp    # one test (.cpp or .c)
plan9/setup_9front.sh     # once: 9front root, kernel, u9fs in ~/9front (needs sudo)
plan9/p9bundle.sh         # pack the translated tests: out/p9bundle/
plan9/run_9front.sh       # boot 9front diskless in QEMU, run the bundle, print results
```
Settings (`common.sh`) can be overridden from the environment: `work out cbe goken nine triple`.
Result on the user's WSL (2026-11-26): goken C++ 8/8, CompCert 23/24 (binarytrees: goken's never-freeing malloc);
**real 9front 32/32**. See ../PLAN.md for what 6c and the Plan 9 libc needed.

## C++ exceptions through the translator
Exceptions are not optional: every C++ test goes this way (`run_phase0.sh` on gcc, `plan9/run_p9.sh` on goken 6c,
`plan9/p9bundle.sh && plan9/run_9front.sh` on real 9front).
Exceptions are lowered in the IR (`opt -wasm-lower-em-ehsjlj -enable-emscripten-cxx-exceptions`): every invoke becomes
a call through an `__invoke_<sig>` wrapper plus a test of `__THREW__`. `eh/mkinvoke.py` generates the wrappers in C
from the lowered IR; `eh/eh_rt.c` is the `__cxa_*` runtime (setjmp/longjmp; `eh/unix` and `eh/plan9` headers);
`eh/eh_match.cpp` matches catch clauses with libc++abi's typeinfo classes, which are translated with the program
(`build_libcxx.sh`, one IR module `out/libcxx.ll`). Result 2026-11-26: e1-e5 identical to native on gcc, goken 6c and
real 9front (bundle 37/37).
Found on the way: a cbe GEP bug (a struct field that is an array, indexed after the `P->field` shortcut, lost its
`.array`), static constructors for kencc (`lower.py` emits `__p9_ctors`), glibc's `__errno_location`,
`__isoc23_strtol` and `__atomic_*` calls in libc_p9.c / rt_p9.c.
