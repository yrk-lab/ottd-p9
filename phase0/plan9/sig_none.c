/*
 * signal() where there are no notes to make signals of (goken): the
 * handler is taken and never runs.  sig9f.c on 9front.
 */
#include <u.h>
#include <libc.h>

void*
signal(int, void*)
{
	return nil;	/* SIG_DFL */
}
