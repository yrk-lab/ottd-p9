# Phase 1: OpenTTD dedicated server under the translator's constraints

Goal: OpenTTD (pinned release, `tag` in `common.sh`, now 15.3) built with clang 17 + libc++, no threads,
dedicated server only (exceptions and RTTI stay: the translator lowers them, see ../PLAN.md), kept as a patch series in `patches/`
so that a new upstream tag means rebasing a few topic patches. Gate: the same savegames, run for the
same number of ticks, give byte-identical saves with the stock build.

```
./setup.sh              # once: clone the tag, export src-ref (no .git, shared .ottdrev), OpenGFX
./build_ref.sh          # stock reference build (system gcc, libstdc++): work/build-ref/openttd
./prep.sh               # src-p9 = src-ref + patches/series
./build_p9.sh           # clang 17 + libc++, exceptions and RTTI on, no threads: work/build-p9/openttd
./gate.sh run work/build-ref/openttd ref
./gate.sh run work/build-p9/openttd p9
./gate.sh cmp ref p9
./mkpatch.sh name file...   # save edits in src-p9 as patches/name.patch
./translate.sh          # OpenTTD through the translator: work/tr/openttd (ir link lower cbe cc)
./gate.sh run work/tr/openttd tr
./gate.sh cmp ref tr
```

The gate (`gate.sh`) runs each scenario with the null video driver (one thread, no network, as fast
as possible) for `ticks` ticks (default 100000, about 3.7 game years) and saves uncompressed on exit:

- `world`: new game from a fixed seed (map generation, towns, industries, economy)
- `regression`: upstream's `regression/regression/test.sav`, loaded without AIs

`savenorm.py` blanks the one field that differs from run to run, the random session id in the DATE
chunk (used by the opt-in survey). With that, two runs of the same build are identical, and so must
two builds be.

Both source trees are exported without `.git` and share one `.ottdrev`, so the stock and patched
builds carry the same version string (it is recorded in the save's game log).

Threads: `nothreads.c`, linked into build-p9, makes `pthread_create` fail, as the Plan 9 runtime will. libc++'s
`std::thread` then throws `std::system_error`, which OpenTTD's `StartNewThread` catches, and its callers run the work
inline. No OpenTTD patch is needed. (`-d misc=1` shows "Can't create thread 'ottd:linkgraph'" and "'ottd:savegame'".)

Status 2026-11-26: clang 17 + libc++, exceptions and RTTI on, no threads, gives saves byte-identical to stock gcc +
libstdc++ with threads (both scenarios). Host tools (strgen, settingsgen) come from the reference build via
`HOST_BINARY_DIR`. `patches/series` is still empty: std::filesystem needs no patch either (libc++'s implementation is
translated; Plan 9 POSIX shims in ../phase0/plan9/libc_p9.c, tested by ../phase0/tests/f1_filesystem.cpp on 9front).

**Translated build (2026-11-26): gate passed.** `translate.sh` recompiles every unit of build-p9 to LLVM IR
(`ir.py`: build flags from compile_commands.json, plus -O1 -g0 -mlong-double-64 -fno-pic -fno-pie, no _FORTIFY_SOURCE,
no __int128 in libc++ and fmt, no precompiled header), links them with what they need of the libc++/libc++abi IR
(28 of 36 modules: iostreams, locale, regex, thread etc. are used), lowers exceptions, runs llvm-cbe (3.9M lines,
132 MB of C, 9 s) and compiles that with gcc -O0 (2.5 min, 5 GB) with the phase0 runtime and nothreads.c. Both
scenarios give byte-identical saves with stock; 100000 ticks take 13 s (stock -O3: 4 s). `externals.txt` lists what
the program needs from the C library and zlib/liblzma: the Plan 9 runtime's to-do list for Phase 2.
The same IR compiled directly by clang (no cbe) is identical too, which is how the cbe bugs were isolated.
