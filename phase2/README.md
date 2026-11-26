# Phase 2: the translated OpenTTD compiled on Plan 9

Input: phase 1's translated program (`../phase1/translate.sh ir link lower` → `work/tr/eh.ll`, whose gcc build
passes the phase 1 gate). Goal: the same program compiled by kencc and run on 9front amd64, with the phase 1
gate's saves matching stock.

```
./build9.sh     # cbe -cbe-explicit-padding -> lower.py --libc -> split.py -> goken 6c -> 6l: work/p9/u/
```

`split.py` cuts the single cbe file (137 MB) into a header (types, prototypes, extern declarations, the static
intrinsic helpers), `data.c` (the globals) and about 30 function units of ~4 MB. The static names become global:
LLVM made module-level names unique, and C++ file-local names are mangled.

## Status (2026-11-26)

- **Compiles**: all 32 units compile with goken's 6c, no errors, 45 s with 4 jobs. (The whole file as one unit
  also compiles: 6 min, 3.4 GB, too much for a small 9front VM.) The full build takes about 2¼ minutes.
- **No symbol-name hashing needed**: kencc (goken and 9front) has `NSYMB 1500`; OpenTTD's longest identifier is
  626 characters (1,096 are over 255). goken's 6c crashes only beyond ~1,500.
- **Links except for 140 missing symbols** (`missing.txt`): the Plan 9 runtime to write:
  - zlib and liblzma (deflate, inflate, gz*, lzma_*): zlib translated with the program now, liblzma later (Next)
  - stdio: fread, fseek(o), ftell(o), feof, ferror, clearerr, fileno, ungetc, vasprintf, *scanf, flockfile...
  - locale ("C" only): newlocale/uselocale/freelocale, localeconv, catopen/catgets/catclose, strto*_l, strftime_l,
    __ctype_b_loc, __ctype_get_mb_cur_max
  - wide characters: mbrtowc, mbsrtowcs, mbsnrtowcs, mbtowc, wcrtomb, wcsnrtombs, wcslen, wmemchr, fputwc, getwc
  - float math: acosf asinf atanf atan2f expf logf log10f powf tanf modff round
  - pthreads: create fails (as `../phase1/nothreads.c`), mutex/cond/key no-ops
  - process and OS: _exit, dup2, execvp, getuid, getpwuid, uname, sysconf, getrandom, nanosleep, gmtime_r,
    localtime_r, statvfs, fdatasync, ioctl, signals (sigaction, signal, sigprocmask...), strsignal, backtrace,
    dlopen & co (fail)
  - sockets: socket, bind, listen, accept, connect, send/recv(from), getaddrinfo, getnameinfo... (failing stubs
    first; Plan 9 dial/announce later)
  - 1-byte atomics

## Next

Decided 2026-11-26: **zlib is translated with the program now; liblzma is added later.**

Why translate at all: 9front's zlib is APE's libz (zlib 1.2.2, from Ghostscript): its uLong is 32 bits on
Plan 9, so its z_stream layout differs from what the LP64-compiled program expects, and its gz* functions sit
on APE stdio. There is no xz/liblzma port for Plan 9. Translating the C sources with OpenTTD keeps one ABI and
puts the library under the phase 1 gate too.

Why zlib first (a comparison of current zlib and xz 5.8.4):

| | zlib | liblzma |
|---|---|---|
| library code | 15 files, ~24k lines | 94 files, ~42k lines |
| configuration | none (its own zconf.h; 6 optional HAVE_*) | a generated config.h: 77 HAVE_*/TUKLIB_* switches |
| threads | none | multi-threaded encoder/decoder (off: XZ_THREADS=no) |
| SIMD, CPU detection | none in the core | CLMUL CRC with cpuid and `__attribute__((target))`, SSE2 in memcmplen.h, ARM64 CRC: each must be switched off, or it becomes vector code the translator cannot take |
| style | plain portable C89 | C99 with many platform switches |

Why zlib, which is C already, still goes through the pipeline (clang -> IR -> cbe -> lower.py -> 6c) instead
of being compiled by 6c from its own sources: there is nothing to translate from C++, but compiling it natively
would split the program into two ABIs. The translated OpenTTD is compiled for LP64 and lays out z_stream with
64-bit uLong fields (total_in, total_out, adler); zlib compiled by 6c has a 32-bit uLong (Plan 9's long), so the
two sides would disagree on the struct, the same problem as 9front's APE libz. And zlib's gz* functions call
open/read/lseek/close with POSIX flags and types: in the pipeline those go through the same Plan 9 shims as
OpenTTD's own POSIX calls, natively they would reach Plan 9 libc directly. Compiling it natively would mean
patching zconf.h (64-bit uLong and z_off_t) and writing Plan 9 wrappers for its file calls: a small zlib-specific
port to maintain, outside the phase 1 gate. Through the pipeline it costs 15 more units in phase1/translate.sh,
no patches, one integer model for the whole program, and the Linux gate checks it with OpenTTD.

zlib is the cheap, low-risk proof that a third-party C library goes through the translator. What each one
gives OpenTTD: liblzma reads and writes `OTTX` saves, the default format for years, so most saves people have
(and upstream's `regression/test.sav`) need it; zlib reads old `OTTZ` saves, unpacks downloaded content
(`.tar.gz` from BaNaNaS, via gzdopen) and `.gz` readmes. liblzma alone would cover saves but lose content
downloads, and is the harder one to configure. So: zlib now, liblzma afterwards, configured down (decoders
and LZMA2 only, no threads, no CLMUL/ARM64 CRC, no unaligned-access fast paths), so the server can load
ordinary modern saves.

1. zlib (current release) compiled to IR with the program in `../phase1/translate.sh`; liblzma off in the
   phase 1 builds until step 4 (`-DCMAKE_DISABLE_FIND_PACKAGE_LibLZMA=ON`, also for the reference so the builds
   stay alike).
   Without lzma OpenTTD saves with zlib by default and cannot load xz saves. Upstream's
   `regression/test.sav` is xz (`OTTX`): the gate's regression scenario will start from a copy converted
   once by the stock (lzma-capable) build to an uncompressed save. Rerun the phase 1 gate.
2. The rest of `missing.txt` in `../phase0/plan9/libc_p9.c` / `rt_p9.c` (Plan 9 style), with phase 0 tests for
   the parts that can be tested in isolation (stdio, locale, wide chars).
3. Run on goken (Linux), then on 9front with the data files (OpenGFX, lang/), via `../phase0/plan9/run_9front.sh`.
4. liblzma translated the same way, configured down as above; then the gate can use upstream's xz save directly.
