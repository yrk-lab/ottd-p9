/*
 * Thread creation always fails, as it will in the Plan 9 runtime.
 * Linked into the build-p9 executable, this definition is used
 * instead of libc's.  libc++'s std::thread then throws
 * std::system_error, OpenTTD's StartNewThread catches it and returns
 * false, and every caller runs the work inline (link graph jobs,
 * saving, the game loop).  Mutexes keep working.
 */
#include <errno.h>
#include <stdio.h>

int
pthread_create(void *thread, const void *attr, void *(*start)(void*), void *arg)
{
	static int told;

	(void)thread;
	(void)attr;
	(void)start;
	(void)arg;
	if(!told){
		fprintf(stderr, "nothreads: thread creation refused\n");
		told = 1;
	}
	return EAGAIN;
}
