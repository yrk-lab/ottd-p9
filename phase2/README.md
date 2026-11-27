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

## Phase 2.5: unit by unit (2026-11-27)

The whole-program link made every change rewrite 2-3 GB (the linked module, ~150 MB, several times; three
140 MB C files; the split copy; a copy into the 9front root). Now the source's structure is kept: each of
OpenTTD's units (and zlib's, and the libc++ modules the program needs, 538 in all) becomes one flat C file
(`script/api/script_map.cpp` -> `script_api_script_map.c`) and one object, and generated mkfiles rebuild only
what changed. `buildtu.sh` drives it (`../phase0/tu/build.sh`), into `work/tu`:

- `trans.mk` (plan9port's mk, on Linux): each unit's symbol table (`symtab.py`), then, outside mk, the
  shared symbols' homes (`homes.py`), then each unit's translation (`unit.sh`: `localize.py`, the exception
  lowering, llvm-cbe, `lower.py --libc --unit`), its steps in `/dev/shm`, its C file written only if it changed;
- `invoke.c` (the union of the units' exception wrappers, `mergeinvoke.py`) and `ctors.c` (the constructor
  table, `mkctors.py`), also written only if they changed;
- `mkfile` (mk on 9front, plan9port's on Linux): `mk SYS=goken` or `mk SYS=9front`, each with its
  `config.$SYS` and its own object directory.

What a static linker did is now done before translating. C++ puts inline functions, templates, vtables,
typeinfo, static locals and template statics in every unit that uses them (linkonce, weak); 6l has no weak
symbols, so each gets one home unit (the unit with an ordinary definition, else the first in link order),
and elsewhere becomes a declaration (available_externally, then LLVM's elim-avail-extern): one copy of each
static local, one address per function, as C++ requires. Function aliases (C1/D1 for C2/D2) become copies of
their target; the one inline assembly (OpenTTD's cpuid) reads as "no features". The whole program's dead-code
pass no longer hides unused code, so the runtime gained what libc++'s unused parts call (the "C" locale's
`_l` functions, wide conversions, file calls, `exception_ptr`), and llvm-cbe two fixes (the structs its
overflow helpers return; extern arrays of unknown size).

A build with nothing changed does nothing; a change to one unit translates and compiles that unit and links.
Three systems build the same tree, each into its own object directory:
- `goken` (kencc on Linux): passes the Phase 1 gate (both scenarios identical to the translated Linux
  build, 100000 ticks);
- `9front`: built in place. `run9front.sh` serves the repository with a second u9fs (u9fs chroots to what it
  serves and looks users up in its etc/passwd, so the repository is bind-mounted into a small tree that has
  one) and `build.rc` runs `mk SYS=9front` in it: nothing is copied, objects persist across boots. A full
  build is 10 minutes, the link 6; a boot after a one-file change about 7 minutes in all. The gate passes
  (`gate.sh cmp tr 9front`: same world and regression saves), so do the socket test and the dedicated
  server with the stock client;
- `linux` (`sys="goken linux" buildtu.sh`): gcc and glibc, for the Phase 3 loop with plan9port's libdraw.
  The units' `c_` names are glibc's again (`unrename.h`), `main_unix.c` runs the constructors, and
  `-fpack-struct` is the 6c `#pragma pack on` that gcc ignores (cbe pads every struct explicitly; unpacked,
  a libc++ tree node grew and the heap broke). Passes the gate too.

Names the Plan 9 libraries define as well as the program: libdraw's `display`, `font`, `screen` and
`_screen` (OpenTTD has a global `_screen`; C++ does not mangle namespace-scope variables) are renamed
`p_<name>` by lower.py. 9front's libc has no `openat`, goken's does: `os9f.c` is linked on 9front only.

## Sockets: the dedicated server takes a client (2026-11-26)

The BSD socket calls run on Plan 9 networking (`../phase0/plan9/net_p9.c`): TCP and UDP over IPv4, through
dial, announce, listen and accept. OpenTTD polls (non-blocking accept/recv/recvfrom, select with no timeout)
where Plan 9 blocks, so each receiving socket has a helper that blocks for it and fills the socket's buffers;
the calls look only at those. The fd the program sees is the conversation's ctl. Helpers never call malloc,
so one killed at close holds no lock but its socket's. A connected socket's reader is ended by TCP's
graceful `close` ctl message (FIN, and the reader's read returns); closing the descriptors is not enough, as
the blocked read keeps the conversation open (tried: the peer never sees end of stream).

The system part is a file of its own, as rd's TLS (`tls9f.c`/`tlsp9p.c`): `net9f.c` on 9front (helpers are
rfork(RFMEM) processes, UDP is /net's header mode, names go to /net/cs) and `netp9p.c` with plan9port
(threads, udpread/udpwrite), behind `net.h`. goken has no dial or announce: its builds link `net_none.c`,
the failing stubs. lower.py renames the whole socket API (and close, ioctl) to `c_<name>`, so ours never take
the place of the C library's, which plan9port's lib9 uses.

Tests (`../phase0/tests_net/n1_sock.c`: listen, non-blocking accept, connect, data both ways, end of stream,
UDP both ways), compared with the same test on Linux:
- fast loop, seconds: `../phase0/plan9/run_net.sh` builds it with plan9port against net_p9.c + netp9p.c;
- 9front: `only=net run9front.sh` (under 2 minutes), the test translated through the pipeline.

The full `run9front.sh` then starts `openttd -D` on 9front, and the stock Linux build joins it as a client
through QEMU's port forward: it joins, starts a company and plays for 60 s; no desync on either side. A
diskless 9front boot has no loopback interface: termrc.local adds it (127.1), as termrc does on a disk.

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
