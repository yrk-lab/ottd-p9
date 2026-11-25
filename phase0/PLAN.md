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
2. **Compile the C on Plan 9.** 2a: old gcc under APE. 2b: patch llvm-cbe to a Plan 9 C dialect (no inline, vlong, C89 decls,
   no GNU attributes) + symbol-name hashing. Gate: dedicated server runs natively on 9front amd64 (QEMU).
3. **Video driver.** `src/video/plan9_v.cpp` (translated C++) + small hand-written C shim on libdraw (initdraw, loadimage,
   mouse/keyboard, eresized). Start as split processes (game under APE, native frontend over a pipe), merge later if wanted.
   Built-in sprite font, null sound, 32bpp-optimized blitter, OpenGFX base set.
4. **Upstream-tracking CI.** Script bumps the tag, regenerates C, builds, reruns the Phase 1 savegame check.

## Plan B (if cbe route fails)
clang -> freestanding x86-64 objects -> lld at Plan 9 load addresses -> a.out wrapper, with a syscall/ABI shim
(SysV vs Plan 9 calling convention, TLS). More robust, but not a translator.

## Risks
- Generated C is huge and unreadable: keep `-g` so `#line` maps back to source.
- libc++ assumes a fairly complete POSIX libc: APE-first.
- Plan 9 enables FPU exceptions by default: mask them early in `main`.
- Split-process frontend adds latency (fine at OpenTTD frame rates).
