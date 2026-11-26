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
- **Links** (since the runtime below; it lacked 140 symbols, 123 once zlib was inside). What was written:
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

## On 9front: the gate passes (2026-11-26)

`run9front.sh` stages the units, the runtime and the data in the 9front root, boots 9front diskless under
QEMU/KVM (u9fs), and `build.rc` compiles with 9front's own 6c (9 minutes, two jobs), links with 6l (6
minutes; a 42 MB `openttd.out`), runs `openttd -h`, and plays the two gate scenarios (70 s together). The
saves, normalized on the host, are byte-identical to the Linux builds' (`../phase1/gate.sh cmp tr 9front`):
world and regression, 100000 ticks each. Not yet: the dedicated server (sockets are failing stubs; Plan 9
dial/announce under the BSD calls is next), liblzma.

## Plan 9 runtime: links, runs, passes the gate on goken (2026-11-26)

The missing symbols are written (`../phase0/plan9`): `libc_p9.c` (stdio including fread/fseek/ungetc/
vasprintf and scanf, the "C" locale, wide characters in the "C" locale, float math), `os_p9.c` (POSIX
files, process, time; signals, dl*, sockets as failing stubs), `rt_p9.c` (1-byte atomics, pthreads: create
fails as `../phase1/nothreads.c`, the rest no-ops). The link is complete: a 43 MB `openttd.exe`.

The kencc build, run on Linux (goken's binaries are Linux ELF), plays both gate scenarios for 100000 ticks
and gives saves byte-identical to the reference, native and translated Linux builds. What it took:

- **Member-function pointers** (`../phase0/eh/pmf.py`): the Itanium C++ ABI marks a virtual member pointer
  by bit 0 and assumes functions at even addresses; Plan 9's linker does not align functions, so a non-virtual
  member function at an odd address was called through the vtable (the first crash, in `std::call_once`).
  The test becomes "bit 0 set and below the text base" (0x200000); 130 call sites in OpenTTD.
- **glibc locale object**: libc++'s `classic_table()` is `__cloc()->__ctype_b`, read from glibc's
  `struct __locale_struct`; `newlocale` returns one with the classification and case tables.
- **fmt to a FILE**: fmt's glibc path writes into the FILE's own buffer (`_IO_write_ptr` and so on), which
  the runtime's FILE does not keep: `-DFMT_USE_FALLBACK_FILE=1` (`../phase1/ir.py`) makes it use fwrite and
  putc. The flag is in the Linux translated build too, which still passes the gate.
- **goken's malloc** is a placeholder (a 64 MB bump allocator; free does nothing, realloc copies the new
  size), and its sbrk hands out unmapped memory when Linux randomizes the break: goken builds link
  `malloc_goken.c` (power-of-two classes with free lists, on the real break). Not used on 9front, whose libc
  has the pool allocator.
- 9front's libc.h declares bind, listen and accept: the program's are renamed (c_bind...) with the other
  libc shims.
- The data directories (lang, baseset...) are linked next to openttd.exe, as for the Linux builds.

## zlib inside, liblzma off (2026-11-26)

Step 1 below is done: zlib 1.3.1 is compiled to IR with OpenTTD (`../phase1/translate.sh`, 15 units, no -lz),
liblzma is off in the reference, native and translated builds, and the gate's regression scenario starts from
upstream's save made uncompressed (`../phase1/unxz.py`; checked against loading the xz original with an
lzma-capable stock build: same result). The phase 1 gate passes: reference, native and translated builds give
byte-identical saves. The translated program has 202 externals (`../phase1/externals.txt`), and the kencc
link now lacks 123 symbols (`missing.txt`), none from zlib or liblzma.

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
