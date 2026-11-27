# Phase 3: the video driver

OpenTTD's own video driver for Plan 9, `src/video/plan9_v.cpp` (in `../phase1/patches/plan9-video.patch`), is
translated with the rest of the game. It draws with the 32 bpp blitter into a buffer and talks to a small
hand-written C shim on libdraw through ints and pointers only (`../phase0/plan9/p9draw.h`):

- `draw_p9.c`: the window, flushing the dirty rectangle (whole rows, `loadimage` into an XRGB32 image, `draw`
  onto the screen), the mouse, resizes, the hidden Plan 9 cursor (the game draws its own);
- the keyboard, the system part: `kbd9f.c` on 9front reads `/dev/kbd` (keys down and up, so held keys and
  modifiers work), `kbdp9p.c` with plan9port takes event(2)'s characters only;
- `draw_none.c`: for builds with no draw device (goken), `p9draw_init` fails and the game picks another driver.

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

The positions in the input scripts are measured from screenshots: the menu's windows are not all centred
alike, and a click a few pixels off lands on another window (on the title screen, Exit quits at once).

While it runs, rio's windows can be looked at from the host: `./vmsh 'mount /srv/rio.glenda.* /n/rio 1;
cat /n/rio/wsys/*/label'` (rio wants a window id as attach name; reading a wctl twice waits for the next
change).

The title screen's game plays (liblzma, ../phase2/README.md).  Not yet: sound and music (null drivers).
