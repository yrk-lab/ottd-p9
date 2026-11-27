/*
 * The plan9port keyboard of the draw shim, for the fast test loop:
 * event(2)'s keyboard, characters only.  Without keys down and up the
 * game sees no modifiers and no held direction keys; arrows still
 * arrive as key presses.  See p9draw.h.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <event.h>
#include "p9draw.h"

ulong
kbdevents(void)
{
	return Ekeyboard;
}

void
kbdinit(void)
{
}

int
kbdevent(P9Event *e)
{
	if(!ecankbd())
		return 0;
	memset(e, 0, sizeof *e);
	e->type = P9Echar;
	e->rune = ekbd();
	return 1;
}
