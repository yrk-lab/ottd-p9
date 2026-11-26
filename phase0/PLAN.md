# OpenTTD for Plan 9: translation plan

**Goal:** run OpenTTD on Plan 9 (9front amd64), with upstream updates adopted by re-running a pipeline, not by hand-porting.

## Findings that shape the plan
- OpenTTD requires C++20 (since 14.0). cfront (~C++ 2.x/3.0: no full templates, no STL, no lambdas) cannot be extended to that: dead end.
- No maintained modern C++-to-C source translator exists; C++-to-Go for a code base this size and style is not realistic.
  Go's Plan 9 port is itself still "experimental"; status of Go draw libs on *native* Plan 9 unverified.
- Viable automatic route: **clang -> LLVM IR -> llvm-cbe -> C**. Known limits: no exception handling in C, GNU-flavoured output,
  some intrinsics unsupported (patched as needed).
- The generated C is GNU C; 6c/8c/pcc won't accept it as-is. Old gcc ports (ports2plan9/APE) exist but are stale.

## Pipeline
```
OpenTTD tag -> clang (C++20, -fno-exceptions -fno-rtti, fixed triple)
            -> per-TU LLVM IR (+ libc++ built to IR) -> llvm-link / globaldce
            -> llvm-cbe (patched; Plan 9 dialect + name hashing later)
            -> C -> mk on Plan 9
```
Hand-written pieces only: Plan 9 video driver, small OS layer, patch series for OpenTTD (file layer, exception sites).

## Phases and gates
0. **Feasibility** (toy C++ incl. STL through the pipeline, run on Linux). Gate: outputs identical to native build. *(this harness)*
1. **Dedicated server, no graphics.** `OPTION_DEDICATED=ON`, no threads, no ICU/harfbuzz/freetype/fontconfig/curl/SDL/fluidsynth.
   Build zlib + liblzma yourself; libc++ without threads/localization/filesystem (patch OpenTTD's `std::filesystem` use);
   rewrite the few try/catch sites. Gate: loads a savegame, ticks N days, identical checksums vs upstream build (on Linux).
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
- Next corpus: CompCert benchmarks through the pipeline; then goken `tests/c` as a regression check on the lowering itself.
Status: all 8 tests (t1-t3, m1-m5) identical to native through goken 6c. 9front result: pending.

### What kencc needed (all mechanical, all now scripted)
- No stdint.h, #if, __attribute__, function-type typedefs, `##`, __builtin_*: handled by `plan9/lower.py` + `plan9/shim/` (to become a dialect switch inside llvm-cbe).
- **Struct layout:** 6c rounds every record up to the word (8 bytes), so LLVM-computed offsets break. Fix: cbe `-cbe-explicit-padding` emits each gap as an explicit byte array
  and wraps *every* record, including the `l_array_N_T` wrapper structs, in `#pragma pack on/off` (in this kencc `pack on` = true packing). Missing the array wrappers made
  `[4 x i8]` tail padding 8 bytes and vector<Item> grew to the wrong capacity: found by bisecting with single-feature tests (tests_p9/m1-m5).
- 64-bit literals: emit `...ull`/`...ll` directly. 6c warns `truncated constant` for pasted forms.
- Plan 9 libc differences (printf->print, no %zu, bcmp, ceilf) live in `plan9/rt_p9.c`.
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
