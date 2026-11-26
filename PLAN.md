# OpenTTD for Plan 9: translation plan

**Goal:** run OpenTTD on Plan 9 (9front amd64), with upstream updates adopted by re-running a pipeline, not by hand-porting.

## Findings that shape the plan
- OpenTTD requires C++20 (since 14.0). cfront (~C++ 2.x/3.0: no full templates, no STL, no lambdas) cannot be extended to that: dead end.
- No maintained modern C++-to-C source translator exists; C++-to-Go for a code base this size and style is not realistic.
  Go's Plan 9 port is itself still "experimental"; status of Go draw libs on *native* Plan 9 unverified.
- Viable automatic route: **clang -> LLVM IR -> llvm-cbe -> C**. Known limits: GNU-flavoured output, some intrinsics
  unsupported (patched as needed). C has no exceptions: they are lowered in the IR (below), so OpenTTD keeps its own.
- The generated C is GNU C; 6c/8c/pcc won't accept it as-is. Old gcc ports (ports2plan9/APE) exist but are stale.

## Pipeline
```
OpenTTD tag -> clang (C++20, exceptions and RTTI on, fixed triple)
            -> per-TU LLVM IR (+ libc++ and parts of libc++abi built to IR) -> llvm-link / globaldce
            -> Emscripten-style exception lowering (opt -wasm-lower-em-ehsjlj): invokes become calls
               through generated __invoke_ wrappers + a __THREW__ test, all plain IR
            -> llvm-cbe (patched; Plan 9 dialect + name hashing later)
            -> C + eh runtime (setjmp/longjmp, phase0/eh) -> mk on Plan 9
```
Hand-written pieces only: Plan 9 video driver, small OS layer, patch series for OpenTTD (file layer, threads).

**Exceptions (decided 2026-11-26, Phase 1):** translated, not patched out. With `-fno-exceptions` OpenTTD 15.3 has
133+ throw / 34 try sites (half in the script engine, which unwinds non-locally through Squirrel), so patching would
stub scripting and make every upstream bump re-port them. Instead the IR is lowered the way Emscripten does it and a
small C runtime implements the Itanium `__cxa_*` ABI with setjmp/longjmp: each `__invoke_<sig>` wrapper (generated per
signature by `phase0/eh/mkinvoke.py`) pushes a frame and setjmps, a throw longjmps to the innermost frame, and cleanups
and handlers stay ordinary code, so destructors run. Catch matching uses libc++abi's own typeinfo classes (translated).
Cost: a setjmp per call site that has a cleanup or handler. Proven by `phase0/tests_eh` (throw/catch, class hierarchies
and pointer adjustment, unwinding with RAII, rethrow, libc++'s own exceptions, OpenTTD-like patterns): identical to
native on gcc, goken 6c and **real 9front**. **RTTI** stays on too (about 80 `dynamic_cast`s, mostly window code): it
comes with libc++abi's typeinfo code, which the exception runtime needs anyway.

## Phases and gates
0. **Feasibility** (toy C++ incl. STL through the pipeline, run on Linux). Gate: outputs identical to native build. *(this harness)*
1. **Dedicated server, no graphics.** `OPTION_DEDICATED=ON`, no threads, no ICU/harfbuzz/freetype/fontconfig/curl/SDL/fluidsynth.
   Build zlib + liblzma yourself; libc++ without threads/localization/filesystem (patch OpenTTD's `std::filesystem` use).
   Exceptions and RTTI stay (see Pipeline). Gate (`phase1/gate.sh`): the same savegames, run the same number of ticks,
   give byte-identical saves vs the stock upstream build. OpenTTD 15.3; clang 17 + libc++ already matches stock gcc.
2. **Compile the C on Plan 9.** Primary path (proven small by Phase 0.5): a Plan 9 dialect mode in llvm-cbe (what `lower.py` does today + explicit padding)
   + symbol-name hashing. Old gcc under APE is only a fallback. Gate: dedicated server runs natively on 9front amd64 (QEMU).
3. **Video driver.** `src/video/plan9_v.cpp` (translated C++) + small hand-written C shim on libdraw (initdraw, loadimage,
   mouse/keyboard, eresized). Start as split processes (game under APE, native frontend over a pipe), merge later if wanted.
   Built-in sprite font, null sound, 32bpp-optimized blitter, OpenGFX base set.
4. **Upstream-tracking CI.** Script bumps the tag, regenerates C, builds, reruns the Phase 1 savegame check.

## Phase 0.5: Plan 9 compiler acceptance (added after research; gate before Phase 1)
Research result: there is no standalone C-compiler test suite maintained by Forsyth. His compiler lineage (ken-cc -> fork-kencc -> goken9cc by aryx)
is what builds the Plan 9 compilers on Linux; the 2010 Go tree bundled the same C toolchain (its test/ dir tests Go, not C). goken9cc ships ~200 small
C regression tests (tests/c/*) and 24 CompCert benchmark programs (benchs/compcert) usable as an acceptance corpus.
Caveat: goken forks an older compiler without many 9front fixes, so goken passing is the fast loop, not the truth. **Real 9front 6c is the gate.**
- Fast loop (Linux): `plan9/setup_goken.sh` then `plan9/run_p9.sh` (clang -> IR -> cbe `-cbe-explicit-padding` -> `lower.py` -> 6c -> 6l + goken libc -> run -> diff vs native).
- Truth (your machine): `plan9/p9bundle.sh` collects lowered C + shims; copy `out/p9bundle/` into a 9front VM and run `rc run9front.rc` (untested script).
- C corpus: `plan9/run_compcert.sh` (goken's 24 CompCert benchmarks, ISO C) through `plan9/p9check.sh` (one script for .cpp and .c): clang (C) -> IR -> cbe -> `lower.py --libc`
  -> 6c -> 6l + `plan9/libc_p9.c` + goken libc; output must equal native clang *and* the reference `Results/` file.
Status (2026-11-26, user's WSL, Ubuntu 26.04): all 8 C++ tests identical to native through goken 6c; CompCert 23/24. The 24th, binarytrees, matches
native up to depth 12 and then exhausts goken's 64 MB never-freeing bump malloc (a goken libc limit, not the pipeline). goken's own `tests/c` (`mk test`,
all arches under qemu-user) passes, i.e. the 6c we test against is healthy. tests/c is Plan 9-dialect compiler-bug repros, so it is a 6c check,
not a translator check.
9front (release 11952, diskless boot with root over u9fs: `plan9/setup_9front.sh` once, then `plan9/p9bundle.sh && plan9/run_9front.sh`):
**PASS 32/32 (8 C++ tests + 24 CompCert, binarytrees included), 3.5 minutes with KVM. Phase 0.5 gate passed on real 9front.**
Getting there needed C++ to use `libc_p9.c` too: the earlier printf->print mapping worked on goken only (`%u`: a verb in goken, a flag in 9front).

### What kencc needed (all mechanical, all now scripted)
- No stdint.h, #if, __attribute__, function-type typedefs, `##`, __builtin_*: handled by `plan9/lower.py` + `plan9/shim/` (to become a dialect switch inside llvm-cbe).
- **Struct layout:** 6c rounds every record up to the word (8 bytes), so LLVM-computed offsets break. Fix: cbe `-cbe-explicit-padding` emits each gap as an explicit byte array
  and wraps *every* record, including the `l_array_N_T` wrapper structs, in `#pragma pack on/off` (in this kencc `pack on` = true packing). Missing the array wrappers made
  `[4 x i8]` tail padding 8 bytes and vector<Item> grew to the wrong capacity: found by bisecting with single-feature tests (tests_p9/m1-m5).
- 64-bit literals: emit `...ull`/`...ll` directly. 6c warns `truncated constant` for pasted forms.
- C++ runtime (new/delete, pure virtual, libc++ abort) is `plan9/rt_p9.c`; every test links it and `plan9/libc_p9.c`.
- **libc ABI:** the IR is x86_64 Linux (LP64), but on Plan 9 amd64 `long` is 32 bit, so Plan 9's strtol/atol etc. return the wrong width and `%ld`
  prints 32 bits. Plan 9 has no printf/FILE, and integer verbs differ between libcs (`%x` is signed; `u` is a flag in 9front but a verb in goken).
  `lower.py --libc` renames ISO C stdio/long-returning names to `c_*`, implemented in `plan9/libc_p9.c` on Plan 9 libc only (own integer
  formatting, floats via print, buffered stdout flushed by exit, `main` wrapped so the return value becomes the exit status).
- **cbe bug fixed (explicit padding):** static struct initializers did not supply values for the `padN[]` members, so values shifted into the pad arrays
  (6c error in sha3; with brace elision it could be a *silent* misassignment). The patch now emits `{ 0 }` for each gap.
- `__builtin_unreachable()` lowers to `for(;;) abort()`: 6c rejects a non-void function that ends in plain `abort()` ("no return at end of function").
- Verified: 7 integer helper functions agree between gcc and 6c over ~1,100 input pairs each.
- Observed risk: goken 6c was killed (likely out of memory) on one ~4,000-statement straight-line function (a test artefact, fine when split). OpenTTD has large functions, so watch 6c memory at scale.

## Plan B (if cbe route fails)
clang -> freestanding x86-64 objects -> lld at Plan 9 load addresses -> a.out wrapper, with a syscall/ABI shim
(SysV vs Plan 9 calling convention, TLS). More robust, but not a translator.

## Risks
- Generated C is huge and unreadable: keep `-g` so `#line` maps back to source.
- libc++ assumes a fairly complete POSIX libc: APE-first.
- Plan 9 enables FPU exceptions by default: mask them early in `main`.
- Split-process frontend adds latency (fine at OpenTTD frame rates).
