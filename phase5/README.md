# Phase 5: 9legacy (issue #19)

The translated OpenTTD on 9legacy: Plan 9 from Bell Labs, 4th edition, with the 9legacy patches, on amd64.

## The system

9legacy's CD image (http://9legacy.org/download/9legacy.iso.bz2, sha256 fb7e5c9e…, 2026-11-26) has 386, amd64
and arm binaries; its amd64 kernel is 9k's k10: `amd64/9k10f` is a terminal (draw with VESA, mouse, kbd, ether,
sd, and virtio9p). `run9legacy.sh` boots it directly in QEMU (multiboot, `-kernel amd64/9k10f.elf`), its root
the CD image's tree on the host (`work/root`) and this repository a second device (`#9/1`, on `/n/ottd`), both
over virtio9p (`nobootprompt=virtio9p`). QEMU's 9p devices must be modern-only (`disable-legacy=on`): the driver
matches only the virtio 1.0 id. termrc.local, written by run9legacy.sh, brings the network up (termrc would,
after it), starts vmsh's listener, runs `$script` and reboots, which ends QEMU; with `keep=1` the VM stays, and
`../phase2/vmsh` is a shell in it.

	script=phase5/build9l.rc ticks=100000 ./run9legacy.sh
	script=phase5/build9l.rc gui=1 guishots="30 50" ./run9legacy.sh

`build9l.rc` builds FreeType (a tree of its own, `../phase1/work/freetype9l`: 9front's objects are another
compiler's) and the translated game (`mk SYS=9legacy` in `../phase2/work/tu`, objects in `obj.9legacy`) with
9legacy's own 6c, then plays the gate's scenarios, runs the thread test t2 ten times, starts a dedicated server
that run9legacy.sh joins with the stock Linux client, and, with `gui=1`, runs the game on the bare screen and
takes screenshots (`work/9l/screen.t.png`). run9legacy.sh compares the saves with the reference build's.

## The differences, as system parts

- Atomics: 9legacy has no atomic(2), only `cas32`, `casl` and `casp`. `../phase0/plan9/atom_p9.c` (`tcas`,
  `tadd`, the `__atomic_*` calls) is written on a 32-bit and a 64-bit compare and swap: `atom9f.c` (9front's
  atomic(2)), `atom9l.c` (9legacy's `cas32`, and `casp`, a pointer's size on amd64).
- The keyboard: no `/dev/kbd`, so no key-up events: `kbdp9p.c`, event(2)'s characters, as with plan9port.
- No audio device on amd64: `audio_none.c`.
- Everything else is 9front's: `thread9f.c` (semacquire, tsemacquire, privalloc are there), `sig9f.c`,
  `net9f.c`, `cons9f.c`, `win9f.c`, `plumb9f.c`, `os9f.c`.

`../phase0/tu/build.sh` writes `config.9legacy` with those.

## What 4e is like

- No `cut`: the scripts use awk.
- rc forks again for an environment assignment before a command (`home=$d cmd &`) and for `@{...}`: `$apid` is
  then another rc, and an interrupt sent there misses the program. build9l.rc finds the server with ps.
- The VESA BIOS's real-mode calls go through `aux/realemu` on amd64, as termrc does for `monitor=vesa`; without
  it `aux/vga` falls back to the generic `vga` type, which k10f does not have. k10's VESA driver keeps a soft
  screen (vgactl's `addr` is 0).

## Checked

- The gate at 100000 ticks: world and regression the same as the reference build's.
- t2: 10 of 10.
- The dedicated server: the stock Linux client joined and stayed, no desync; the server quit on the interrupt and
  left nothing behind.
- The GUI on the bare screen: the title screen and its game (`screens/9legacy-title.png`), the game loop in its
  own thread.
