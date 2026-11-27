/*
 * No draw device: the draw shim's calls (p9draw.h) for builds without
 * one, so OpenTTD's Plan 9 video driver fails to start and another is
 * used.  Linked into goken builds and into the Linux builds of phase1
 * (native and translated).  No includes, so any C compiler takes it.
 */

int
p9draw_init(char *label, int *w, int *h)
{
	*w = 0;
	*h = 0;
	return -1;
}

void
p9draw_exit(void)
{
}

void
p9draw_flush(void *buf, int pitch, int x, int y, int w, int h)
{
}

int
p9draw_event(void *e)
{
	return 0;
}

void
p9draw_warp(int x, int y)
{
}
