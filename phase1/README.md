# Phase 1: OpenTTD dedicated server under the translator's constraints

Goal: OpenTTD (pinned release, `tag` in `common.sh`, now 15.3) built with clang 17 + libc++, no threads,
dedicated server only (exceptions and RTTI stay: the translator lowers them, see ../PLAN.md), kept as a patch series in `patches/`
so that a new upstream tag means rebasing a few topic patches. Gate: the same savegames, run for the
same number of ticks, give byte-identical saves with the stock build.

```
./setup.sh              # once: clone the tag, export src-ref (no .git, shared .ottdrev), OpenGFX
./build_ref.sh          # stock reference build (system gcc, libstdc++): work/build-ref/openttd
./prep.sh               # src-p9 = src-ref + patches/series
./build_p9.sh           # clang 17 + libc++, exceptions and RTTI on: work/build-p9/openttd
./gate.sh run work/build-ref/openttd ref
./gate.sh run work/build-p9/openttd p9
./gate.sh cmp ref p9
./mkpatch.sh name file...   # save edits in src-p9 as patches/name.patch
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

Status 2026-11-26: clang 17 + libc++ gives saves byte-identical to stock gcc + libstdc++ (both scenarios). Host tools
(strgen, settingsgen) come from the reference build via `HOST_BINARY_DIR`. Next: threads, then translating the build.
