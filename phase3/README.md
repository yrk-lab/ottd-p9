# Phase 3: the video driver

OpenTTD's own video driver for Plan 9, `src/video/plan9_v.cpp` (in `../phase1/patches/plan9-video.patch`), is
translated with the rest of the game. It draws with the 32 bpp blitter into a buffer and talks to a small
hand-written C shim on libdraw through ints and pointers only (`../phase0/plan9/p9draw.h`):

- `draw_p9.c`: the window, flushing the dirty rectangle (whole rows, `loadimage` into an XRGB32 image, `draw`
  onto the screen), the mouse, resizes, the hidden Plan 9 cursor (the game draws its own);
- the keyboard, a system part: `kbd9f.c` on 9front reads `/dev/kbd` (keys down and up, so held keys and
  modifiers work), `kbdp9p.c` with plan9port takes event(2)'s characters only;
- the window's state, a system part: `win9f.c` follows rio's `/dev/wctl` (each read after the first waits for a
  change) and tells the driver P9Efocus when the window becomes current or stops being so; `winp9p.c` tells
  nothing (devdraw does not say);
- `draw_none.c`: for builds with no draw device (goken), `p9draw_init` fails and the game picks another driver.

How keys reach the game: the shim turns the keyboard into events (P9Event). On 9front `kbd9f.c` reads
/dev/kbd: a `c` message (a character typed, repeated while held) is P9Echar; `k` and `K` (the set of keys held,
after one went down or up) become P9Ekeydown and P9Ekeyup. With plan9port, `kbdp9p.c` has event(2)'s characters
only. In the driver only P9Echar makes a key press: `TypeRune` maps the rune (`KeyCode`: Esc, the function and
arrow keys, Enter...) to OpenTTD's key code, adds the modifiers held, and calls `HandleKeypress`; key down and up
only keep the modifiers and the held arrows (`KeyState`), which scroll the map. A control character (0x01 to
0x1A, not Tab, Enter or Backspace) is Ctrl and its letter even where no keys held are told (plan9port): Ctrl+Q,
OpenTTD's quit hotkey, asks in a game and quits at once on the title screen, as upstream (`ctrlq.input`).

Plan 9's interrupt (the Del key where a program reads /dev/cons, as the console and rio's text windows do, or the
note written to /proc/n/note) is the same request. The runtime makes notes signals (`../phase0/plan9/sig9f.c`:
`interrupt`, with or without the newline `echo` adds, is SIGINT, `hangup` SIGHUP, a write on a closed pipe SIGPIPE;
other notes, kill among them, do as always), the video driver takes SIGINT as a quit request (it asks in a game,
quits on the title screen), and the dedicated server's own SIGINT handler quits it. In the game's window Del stays
OpenTTD's key (close the windows): the game reads /dev/kbd, so rio passes it on. build.rc stops the GUI game and
the dedicated server with an interrupt (killing the note group only if one does not quit within 10 seconds) and
checks that each quit on it (an exit status of its own, not the note's) and left no process behind
(run9front.sh's "stops"). With plan9port, SIGINT (Ctrl-C) does the same.

The program's processes end together (issue #21), as libthread's (threadexitsall) and rd's and vncv's do: every
process thread9f.c forks, threads and the shim's helpers alike, is in one list; when the program exits (`c_exit`,
`_exit`, main's `exits`) or a process dies of a note nobody takes, the others are sent a note of their own ("die
openttd kin"), which ends each quietly, and a helper is stopped alone the same way. The note handler is set in
`p9runmain` before anything is forked, as libthread's main does, so every process has it. Not an `atexit` in each
process, as rd's: the atexit table is shared memory here and too small for the threads. Until this, an interrupt
written with echo (`interrupt\n`) matched nothing and killed the main process outright, leaving its helpers
running (`[audio]`, `[music]`, `[net]`: the names of #15 made them visible).

Esc, for example, is rune 0x1B,
`WKC_ESC`: in the game it cancels the tool in hand (the main toolbar's "reset_object_to_place" hotkey), in a query
or a text box it cancels, and it closes the load/save dialog and the high scores; it does not close other windows
(Delete does). In the load/save dialog it takes a second press, as in the stock game (checked against a Windows
build of 15.3).

A struct crossing between translated and native code must have a size that is a multiple of 8: 6c rounds every
struct up to 8 bytes on amd64, clang does not. `P9Event` (five ints) was 24 bytes in the shim and 20 in the game,
and clearing it zeroed half of the caller's return address. It has an explicit pad now.

## Fast loop: plan9port (Linux)

`run_p9p.sh` builds the per-unit tree for Linux (`mk SYS=linux`, ../phase2/README.md) with the shim on
plan9port's libdraw and runs `-v plan9` in a window (WSLg: DISPLAY=:0). A change to the driver rebuilds one
unit and links. With `input=script`, xdotool plays the script once the window is up (`seconds command`, `%w`
for the window); `shots="t..."` are the shim's screenshots, `work/shot.t.png`:

	input=newgame.input shots="17 21 25 40 48 52 58" runtime=62 ./run_p9p.sh

`newgame.input` dismisses the survey, starts a new game, generates the world, presses the arrows, opens the
console and types `help`: all of it works, except that the arrows do not scroll (no key-up events here).

## 9front

`gui=1 ../phase2/run9front.sh` runs the driver on the bare screen (VGA 1024x768x32, no rio; termrc.local runs
screenrc first, since termrc only does after termrc.local). With `guiinput=script`, input goes through QEMU's
monitor (`at x y` moves the PS/2 mouse there, from the corner, in small steps; `click`; or a monitor command
such as `sendkey right 2000`), timed from the shim's window, and `guishots="t..."` are screenshots taken on
that timeline by `screenshot.c | topng` (without rio there is no `/dev/screen`; the tool reads the display's
image), `../phase2/work/9front-screen.t.png`, all on one page in `9front-screens.png` (`sheet.py`):

	ticks=100 guitime=80 guishots="17 21 25 30 40 49 54 58 63 75" \
		guiinput=../phase3/newgame9f.input gui=1 ../phase2/run9front.sh

`newgame9f.input` does the same as on plan9port; on 9front the held right arrow scrolls the map too.
A run that faults leaves the process's stack (acid) in `work/9f/gui/stack`.

### In a rio window

`guirio=1` runs the game as one runs programs on 9front: inside rio, in a window. build.rc starts
`rio -i script`, whose `window -r 40 40 840 640` opens the game's window; with `guiresize=n` a process finds the
window in wsys by its label (the game's caption, "OpenTTD ...") and n seconds later writes `resize -r ...` to
its wctl. Checked: the title game in the window, a resize to 960x700 redrawn at the new size (eresized,
P9Eresize), and `newgame9f-rio.input`: the same session as on the bare screen, the mouse in window coordinates
(survey, New Game, the Generate tooltip, Generate), the held arrow scrolling, the console, `help`.

	ticks=100 gui=1 guirio=1 guitime=85 guishots="44 50 58 64 70 80" \
		guiinput=../phase3/newgame9f-rio.input ../phase2/run9front.sh

In rio the game sees the mouse only while its window is current: rio sends mouse events to the current window
alone (rio.c's mouse thread), so a window under the pointer that is not current gets none, and the first click
only makes it current (issue #14). Every rio program is so. While the window is not current, the shim gives it
rio's cursor back (`esetcursor(nil)`) and the driver takes the game's drawn one away (`UndrawMouseCursor`,
`_cursor.in_window` false, as SDL's driver on leaving its window), so the pointer does not look stuck over a
window that does not hear it (issue #17); when it is current again, the game's cursor comes back with the next
mouse event. Only one reader may open a wctl: without one (the bare screen, or someone else reading it) the
window is taken to be current. Checked:

	ticks=100 gui=1 guirio=1 guitime=85 guifocus="40 60" guishots="35 52 58 70 78" \
		guiinput=../phase3/focus9f-rio.input ../phase2/run9front.sh

`guifocus="t1 t2"`: rio's init script opens a second window at t1 (it becomes current) and makes the game's
current again at t2 through its wctl; the screenshots show the game's cursor, then rio's over the game and none
of the game's, then the game's again. (`screens/9front-rio-focus.png`)

The positions in the input scripts are measured from screenshots: the menu's windows are not all centred
alike, and a click a few pixels off lands on another window (on the title screen, Exit quits at once).

While it runs, rio's windows can be looked at from the host: `./vmsh 'mount /srv/rio.glenda.* /n/rio 1;
cat /n/rio/wsys/*/label'` (rio wants a window id as attach name; reading a wctl twice waits for the next
change).

The title screen's game plays (liblzma, ../phase2/README.md).

## Sound and music (issue #9)

`src/sound/plan9_s.cpp` and `src/music/plan9_m.cpp` (`../phase1/patches/plan9-audio.patch`), translated with
the game, over a shim of ints and pointers (`../phase0/plan9/p9audio.h`):

- sound: there are no threads, so the mixer runs in the game's process, in the sound driver's MainLoop (once a
  tick, as Allegro's): it mixes as much as the shim takes, up to 186 ms ahead, into a ring that a helper process
  (rfork RFMEM) writes to `/dev/audio`, whose writes keep the pace (`audio9f.c`). The mixer itself never leaves
  the game's process: the runtime's mutexes are no-ops.
- music: OpenMSX's songs are MIDI files; `games/dmid` makes them OPL3 instructions and `games/opl3` plays them
  (resampled by its own `pcmconv`). The rc that starts them execs opl3, which reads dmid through `<{}`, so no
  shell waits: `exec games/opl3 <{games/dmid -i bank song}`. A helper reads the pipeline's PCM into a second ring,
  which the mixer takes as its music source (`MxSetMusicSource`, as FluidSynth's driver), so one writer has
  /dev/audio. A full ring holds the pipeline back: music is made as fast as it is played.
- plan9port (`audiop9p.c`): no /dev/audio; with `$p9audio` the sound goes to that file, paced by the clock (no
  music). `audio_none.c` for goken and the native builds: the game takes the null drivers.

setup.sh installs OpenSFX and OpenMSX next to OpenGFX (the gate's saves do not change).

Checked:

	p9audio=$PWD/work/audio.pcm sound=plan9 input=newgame.input shots=30 runtime=40 ./run_p9p.sh
	ticks=100 gui=1 guiaudio=1 guitime=80 guiinput=../phase3/newgame9f.input ../phase2/run9front.sh

The second gives QEMU an Intel HDA card (9front's pc64 kernel has HDA, not AC97; `audio0=type=hda` on the boot
line) whose output QEMU records, `../phase2/work/9front-audio.wav`: the music and the game's sounds, continuous.
While it plays, `./vmsh ps` shows the game's audio writer in Pwrite on /dev/audio and dmid and opl3 waiting on
a full pipe; wadfs serves the bank (`/mnt/wad` is in opl3's `/proc/n/ns`, not the game's), and goes when the
pipeline is killed.

## Names, paths, the bundle

The programs are named as Plan 9 names a binary until it is installed: `6.openttd` (kencc, goken and 9front),
`o.openttd` (gcc: the Linux build of the fast loop, as plan9port's `o.` prefix). The game keeps Plan 9's paths
(`../phase1/patches/plan9-paths.patch`, build_p9.sh): its own directory, configuration, saves and downloads
included, is `$home/lib/openttd`, and installed data is searched in `/sys/lib/openttd`; no XDG directories. The
tests give each run a home of its own (`HOME` on Linux, `home` on 9front).

dmid's instrument bank: 9front comes with one, the GENMIDI lump of its shareware Doom WAD
(`/sys/games/lib/doom/doom1.wad`), which `games/wadfs -r` serves to dmid as `/mnt/wad/genmidi` in the pipeline's
own namespace (`rfork n`), as dmid(1) does it; nothing is copied, and the game's namespace is left alone. A file `genmidi` in a base set directory
takes its place, found the way the game finds its other data. Nothing to set.

`bundle.sh` makes `work/openttd-<version>-plan9-amd64.tgz`: `openttd/` with `6.openttd`, the data it finds next to it, the
licences, OpenTTD's README.md, ours as README.Plan9 (`bundle.README.Plan9`: run in place, or install) and an
mkfile (`bundle.mkfile`): `mk install` puts the binary in `$BIN` (default `$home/bin/$cputype`, as openttd) and
the data in `$home/lib/openttd`; with `BIN` elsewhere (`mk BIN=/$cputype/bin install`) the data goes to
`/sys/lib/openttd`. Both were checked on 9front from the unpacked bundle with a fresh home (`bundletest.rc`,
run in the VM through vmsh): the title game, music with 9front's bank. A test must stop the game by its note group
(`/proc/n/notepg`): libdraw's event processes hold /dev/mouse, and a second game could not open it.

## Threads (issue #11)

OpenTTD runs link graph jobs, world generation and network connects in threads. They are the program's own
pthread calls (libc++'s std::thread, mutex, condition_variable), renamed `c_pthread_*` by lower.py and served by
`../phase0/plan9/thread_p9.c` over a small system part (`thread.h`): `thread9f.c` on 9front, `threadp9p.c` on
Linux with plan9port (glibc's threads: the fast loop), `thread_none.c` on goken (none: thread creation fails and
the work runs inline, as before).

- On 9front a thread is a process sharing memory (rfork RFPROC|RFMEM). Plan 9 gives each such process a private
  stack at the same addresses, but C++ hands stack objects to other threads (a lambda's captures, a condition
  variable a thread waits on: the first t2 run hung on exactly that, each thread waiting on its own copy). So, as
  libthread does, the program runs on stacks in shared memory: main on one (`p9runmain`, from libc_p9's main),
  each new process on its own (`p9proc`, used by the net and audio helpers too). A process is forked from its
  forker's private stack (an rfork on a shared stack would leave two processes on it): p9proc jumps to a point
  p9runmain keeps there. The private stack keeps what is per process: a `privalloc` slot, which has the same
  address in every thread and each thread's contents, holds the thread's state.
- Per thread: errno, the pthread keys, and the exception state (`../phase0/eh/eh_rt.h`, `EH_TLS`: `__THREW__`,
  the innermost handler, the exceptions in flight and caught; lower.py rewrites the units' `__THREW__`).
- Mutexes: a count and a semaphore, the owner and depth (recursive ones). Condition variables: a queue of the
  threads waiting, each on a semaphore of its own (with one semaphore for all, a waiter that came later took the
  first one's wakeup: t2 deadlocked so on 9front). Semaphores are the kernel's (semacquire, tsemacquire), futexes
  on Linux. Atomics: atomic(2) on 9front (bytes by compare-and-swap of the word), gcc's on Linux. Function-local
  statics: a guard with an in-progress flag.
- Tests: `../phase0/tests_tu/t2` (atomics, mutexes, a condition variable with a timed wait, exceptions in every
  thread at once, a contended static), run by `../phase0/tu/test.sh` (goken, Linux) and on 9front through vmsh:
  the native build's output, 30 of 30 runs on Linux, 10 of 10 on 9front. OpenTTD: the gate is the same on
  goken, Linux (18 threads in 5000 ticks) and 9front; the GUI and the dedicated server run.
- Names (issue #15): OpenTTD names its threads (`SetCurrentThreadName`: `ottd:savegame`, `ottd:linkgraph`...,
  through `pthread_setname_np`); `tsetname` gives the name to the system. On 9front it is the process's args
  (`/proc/n/args`, as libthread's threadsetname), which `ps -a` shows after the program's name in brackets, without
  OpenTTD's prefix: `6.openttd [savegame]`; the shim's helpers name themselves `[audio]`, `[music]`, `[net]`.
  With plan9port it is glibc's thread name (`ottd:savegame` in `/proc/pid/task/*/comm`).

## Web links (issue #13)

OpenTTD opens a URL (Help & Manuals' websites, the online content, a readme's links) by running xdg-open; on Plan 9
it plumbs it (`../phase1/patches/plan9-plumb.patch`, `OSOpenBrowser`), and the plumber passes it on as the user's
rules say (9front's basic rules: `http(s)://...` to the web port, a browser). `../phase0/plan9/plumb9f.c` does it
from the game with libplumb; with plan9port `plumbp9p.c` runs its plumb(1) with posix_spawnp (its libplumb needs
libthread, whose main would be the program's); goken and the native builds have none (`plumb_none.c`). Checked:
`plumb.input` on plan9port with a plumber running (`9p read plumb/web`), and `plumb9f.input` on 9front, where
build.rc's GUI stage starts a plumber and keeps what reaches the web port in `work/9f/gui/plumbed`: both got
`https://www.openttd.org/` from the Help window's OpenTTD link.

## The survey's memory size (issue #18)

OpenTTD's survey (its preview: the title screen's question, "Preview survey result") reports the machine's
memory as `sysconf(_SC_PHYS_PAGES)` pages. The runtime (`../phase0/plan9/os_p9.c`) answers that, and
`_SC_AVPHYS_PAGES` (the pages free for programs), from 9front's `/dev/swap` (`bytes memory`, `used/total user`);
where there is none (goken) they fail, as before, and the survey says 0. Checked with `survey9f.input`, which
opens the preview and drags its scrollbar to the end, where `info.os.memory` is ("4 GiB" in the 4 GB VM,
`screens/9front-survey-memory.png`):

	ticks=100 gui=1 guitime=105 guishots="70 90 98" guiinput=../phase3/survey9f.input ../phase2/run9front.sh

A binary just linked is read cold: the title may take 50 s to come up, so the input waits for it.
