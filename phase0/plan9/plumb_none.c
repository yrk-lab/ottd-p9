/*
 * Web links where there is no plumber (goken, the native Linux
 * builds): plumbing fails, and the game says it could not open the
 * link.  plumb9f.c and plumbp9p.c have the plumber.  No includes: the native builds
 * compile it with the system's compiler.
 */

int
p9plumb(char *url)
{
	(void)url;
	return -1;
}
