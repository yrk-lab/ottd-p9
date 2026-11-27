/*
 * Web links: the game's OSOpenBrowser (on Plan 9: patches/
 * plan9-plumb.patch in phase1) plumbs a URL, and the plumber passes it
 * on to what the user's rules say (a browser, or nothing).  On 9front
 * by libplumb, in the game's process; with plan9port (the fast loop)
 * plumbp9p.c runs its plumb(1) instead (its libplumb needs libthread,
 * which wants main); plumb_none.c where there is no plumber (goken,
 * the native Linux builds).
 */
#include <u.h>
#include <libc.h>
#include <plumb.h>

int
p9plumb(char *url)
{
	Plumbmsg m;
	char wd[256];
	int fd, r;

	fd = plumbopen("send", OWRITE);
	if(fd < 0)
		return -1;
	if(getwd(wd, sizeof wd) == nil)
		strcpy(wd, "/");
	memset(&m, 0, sizeof m);
	m.src = "openttd";
	m.dst = nil;
	m.wdir = wd;
	m.type = "text";
	m.attr = nil;
	m.ndata = strlen(url);
	m.data = url;
	r = plumbsend(fd, &m);
	close(fd);
	return r < 0 ? -1 : 0;
}
