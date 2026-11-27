/*
 * The plan9port window state of the draw shim: devdraw does not tell
 * whether the window is current, and its cursor is the system's
 * outside the window anyway: no events.  See p9draw.h.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include "p9draw.h"

void
wininit(void)
{
}

int
winevent(P9Event*)
{
	return 0;
}
