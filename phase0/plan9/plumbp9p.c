/*
 * Web links with plan9port (the fast loop on Linux): its plumb(1)
 * does the plumbing, started with posix_spawnp (not fork: that would
 * copy the whole game).  plan9port's libplumb reaches the plumber
 * through lib9pclient, which needs libthread, whose main would be the
 * program's.  On 9front plumb9f.c plumbs from the game itself.  No
 * Plan 9 headers: this is the system's side.
 */
#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>

extern char	**environ;

int
p9plumb(char *url)
{
	char *argv[3];
	pid_t pid;
	int st;

	argv[0] = "plumb";
	argv[1] = url;
	argv[2] = NULL;
	if(posix_spawnp(&pid, "plumb", NULL, NULL, argv, environ) != 0)
		return -1;
	if(waitpid(pid, &st, 0) < 0 || !WIFEXITED(st) || WEXITSTATUS(st) != 0)
		return -1;
	return 0;
}
