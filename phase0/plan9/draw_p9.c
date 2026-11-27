/*
 * The draw shim under OpenTTD's Plan 9 video driver: the window,
 * flushing the game's 32 bpp screen into it, the mouse, resizes, and
 * the cursor (Plan 9's is hidden: the game draws its own).  The game's
 * pixels are little-endian B G R x words, which is XRGB32.  The
 * keyboard is the system part: kbd9f.c on 9front, kbdp9p.c on
 * plan9port.  See p9draw.h.
 *
 * For tests: with $p9drawshot set to a file name, the window as shown
 * is read back after $p9drawshotat flushes (default 300) and written
 * there as a PPM.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <event.h>
#include "p9draw.h"

static Image	*img;		/* the game's screen, window-sized */
static int	resized;
static int	gone;
static Cursor	blank;
static char	*shot;		/* $p9drawshot */
static int	shotat;
static int	nflush;

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0){
		gone = 1;
		return;
	}
	resized = 1;
}

static int
newimage(void)
{
	if(img != nil)
		freeimage(img);
	img = allocimage(display, Rect(0, 0, Dx(screen->r), Dy(screen->r)), XRGB32, 0, DBlack);
	return img != nil ? 0 : -1;
}

int
p9draw_init(char *label, int *w, int *h)
{
	char *s;

	if(initdraw(nil, nil, label) < 0)
		return -1;
	einit(Emouse | kbdevents());
	kbdinit();
	esetcursor(&blank);
	shot = getenv("p9drawshot");
	if(shot != nil){
		s = getenv("p9drawshotat");
		shotat = s != nil ? atoi(s) : 300;
		free(s);
	}
	if(newimage() < 0)
		return -1;
	*w = Dx(screen->r);
	*h = Dy(screen->r);
	return 0;
}

void
p9draw_exit(void)
{
	esetcursor(nil);
	if(img != nil)
		freeimage(img);
	img = nil;
}

/* the window as shown, as a PPM */
static void
writeshot(void)
{
	Rectangle r;
	uchar *b, *p, *q, c;
	int w, h, n, fd, i;
	Image *t;

	r = screen->r;
	w = Dx(r);
	h = Dy(r);
	t = allocimage(display, Rect(0, 0, w, h), XRGB32, 0, DNofill);
	if(t == nil)
		return;
	draw(t, t->r, screen, nil, r.min);
	n = w * h * 4;
	b = malloc(n);
	if(b == nil || unloadimage(t, t->r, b, n) != n){
		free(b);
		freeimage(t);
		return;
	}
	freeimage(t);
	/* B G R x to R G B */
	for(i = 0, p = b, q = b; i < w*h; i++, p += 4, q += 3){
		c = p[0];
		q[1] = p[1];
		q[0] = p[2];
		q[2] = c;
	}
	fd = create(shot, OWRITE, 0644);
	if(fd >= 0){
		fprint(fd, "P6\n%d %d\n255\n", w, h);
		write(fd, b, w*h*3);
		close(fd);
	}
	free(b);
}

/*
 * Show the rectangle x y w h of the game's screen (pitch pixels a
 * row).  Whole rows are loaded, as they lie in buf.
 */
void
p9draw_flush(u32int *buf, int pitch, int x, int y, int w, int h)
{
	Rectangle r;

	if(img == nil || pitch != Dx(img->r))
		return;
	r = Rect(0, y, pitch, y + h);
	if(loadimage(img, r, (uchar*)(buf + y*pitch), h*pitch*4) < 0)
		return;
	r = Rect(x, y, x + w, y + h);
	draw(screen, rectaddpt(r, screen->r.min), img, nil, r.min);
	flushimage(display, 1);
	if(shot != nil && ++nflush == shotat)
		writeshot();
}

int
p9draw_event(P9Event *e)
{
	Mouse m;

	memset(e, 0, sizeof *e);
	if(gone){
		e->type = P9Eexit;
		return 1;
	}
	if(!resized && ecanmouse()){
		m = emouse();	/* may resize */
		if(!resized){
			e->type = P9Emouse;
			e->x = m.xy.x - screen->r.min.x;
			e->y = m.xy.y - screen->r.min.y;
			e->buttons = m.buttons;
			return 1;
		}
	}
	if(resized){
		resized = 0;
		if(newimage() < 0){
			e->type = P9Eexit;
			return 1;
		}
		e->type = P9Eresize;
		e->x = Dx(screen->r);
		e->y = Dy(screen->r);
		return 1;
	}
	return kbdevent(e);
}

void
p9draw_warp(int x, int y)
{
	emoveto(addpt(screen->r.min, Pt(x, y)));
}
