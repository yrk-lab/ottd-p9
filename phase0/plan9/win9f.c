/*
 * The 9front window state of the draw shim: rio's /dev/wctl, whose
 * first read tells the window's place and state at once and each
 * later one waits for a change:
 *	minx miny maxx maxy current|notcurrent visible|hidden
 * (each padded to 12 characters).  A change of current becomes
 * P9Efocus.  Only one may read wctl: if it cannot be opened (the bare
 * screen, or someone reads it already), the window is taken to be
 * current.  See p9draw.h.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <event.h>
#include "p9draw.h"

static ulong	winkey;		/* the event key of /dev/wctl, or 0 */
static int	current = 1;

void
wininit(void)
{
	int fd;

	fd = open("/dev/wctl", OREAD);
	if(fd >= 0)
		winkey = estart(0, fd, 128);
}

int
winevent(P9Event *e)
{
	Event ev;
	char buf[129], *f[6];
	int n, c;

	while(winkey != 0 && ecanread(winkey)){
		eread(winkey, &ev);
		n = ev.n;
		if(n > sizeof buf - 1)
			n = sizeof buf - 1;
		memmove(buf, ev.data, n);
		buf[n] = 0;
		if(tokenize(buf, f, nelem(f)) < 5)
			continue;
		c = strcmp(f[4], "current") == 0;
		if(c != current){
			current = c;
			e->type = P9Efocus;
			e->x = c;
			return 1;
		}
	}
	return 0;
}
