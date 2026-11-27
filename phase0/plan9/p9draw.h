/*
 * The shim under OpenTTD's Plan 9 video driver (src/video/plan9_v.cpp,
 * patches/plan9-video.patch in phase1), shared by draw_p9.c and its
 * keyboard part: kbd9f.c on 9front, kbdp9p.c on plan9port.  The
 * driver declares the same P9Event and calls; draw_none.c stands in
 * where there is no draw device.  Include after <u.h>, <libc.h>,
 * <draw.h>, <cursor.h> (9front) and <event.h>.
 */

typedef struct P9Event P9Event;
struct P9Event
{
	int	type;
	int	x;		/* P9Emouse: position in the window; P9Eresize: size */
	int	y;
	int	buttons;	/* P9Emouse: 1 left, 2 middle, 4 right, 8 16 the wheel */
	int	rune;		/* P9Echar, P9Ekeydown, P9Ekeyup */
};

enum
{
	P9Enone,
	P9Emouse,
	P9Echar,	/* a character typed, or repeated */
	P9Ekeydown,	/* a key went down: state only */
	P9Ekeyup,
	P9Eresize,
	P9Eexit,	/* the window is gone */
};

/* kbd9f.c, kbdp9p.c */
ulong	kbdevents(void);	/* einit's keyboard bit, if the events have it */
void	kbdinit(void);
int	kbdevent(P9Event*);
