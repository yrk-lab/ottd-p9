/*
 * screenshot - the screen, as an image(6) file on standard output
 *	for topng(1): screenshot | topng >shot.png.  Without rio there
 *	is no /dev/screen; this reads the display's image through the
 *	draw device instead.  initdisplay, not initdraw: on a bare screen
 *	initdraw would put a window of its own over the whole screen.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>

void
main(int, char**)
{
	Display *d;

	d = initdisplay(nil, nil, nil);
	if(d == nil)
		sysfatal("initdisplay: %r");
	if(writeimage(1, d->image, 0) < 0)
		sysfatal("writeimage: %r");
	exits(nil);
}
