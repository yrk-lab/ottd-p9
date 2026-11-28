/*
 * The plan9port part of the program's threads (thread_p9.c, thread.h),
 * for the fast loop on Linux: the threads are glibc's, which the
 * program reaches only through thread_p9.c (lower.py renames its
 * pthread calls), semaphores futexes in an int, atomics gcc's.  No
 * Plan 9 headers: this is the system's side.  errno is glibc's, each
 * thread's already.
 */
#define _GNU_SOURCE
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

typedef long long vlong;
#include "thread.h"

typedef struct Start Start;
struct Start
{
	Thread	*t;
	void	(*fn)(Thread*);
};

static __thread Thread	*me;

Thread*
tself(void)
{
	return me;
}

static void*
trampoline(void *a)
{
	Start s;

	s = *(Start*)a;
	free(a);
	me = s.t;
	s.fn(s.t);
	return NULL;
}

int
tstart(Thread *t, void (*fn)(Thread*))
{
	pthread_t id;
	Start *s;

	s = malloc(sizeof *s);
	if(s == NULL)
		return -1;
	s->t = t;
	s->fn = fn;
	if(pthread_create(&id, NULL, trampoline, s) != 0){
		free(s);
		return -1;
	}
	pthread_detach(id);	/* thread_p9.c joins by its semaphore */
	return 0;
}

static vlong
now(clockid_t c)
{
	struct timespec ts;

	clock_gettime(c, &ts);
	return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void
semup(int *s, int n)
{
	__atomic_add_fetch(s, n, __ATOMIC_SEQ_CST);
	syscall(SYS_futex, s, FUTEX_WAKE, n, NULL, NULL, 0);
}

int
semdown(int *s, vlong ms)
{
	struct timespec rel;
	vlong end, left;
	int v;

	end = ms >= 0 ? now(CLOCK_MONOTONIC) + ms * 1000000LL : 0;
	for(;;){
		v = __atomic_load_n(s, __ATOMIC_SEQ_CST);
		while(v > 0)
			if(__atomic_compare_exchange_n(s, &v, v - 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
				return 1;
		if(ms < 0){
			syscall(SYS_futex, s, FUTEX_WAIT, 0, NULL, NULL, 0);
			continue;
		}
		left = end - now(CLOCK_MONOTONIC);
		if(left <= 0)
			return 0;
		rel.tv_sec = left / 1000000000LL;
		rel.tv_nsec = left % 1000000000LL;
		syscall(SYS_futex, s, FUTEX_WAIT, 0, &rel, NULL, 0);
	}
}

int
tcas(int *p, int old, int new)
{
	return __atomic_compare_exchange_n(p, &old, new, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

int
tadd(int *p, int v)
{
	return __atomic_add_fetch(p, v, __ATOMIC_SEQ_CST);
}

void
tyield(void)
{
	sched_yield();
}

vlong
tnow(int clock)
{
	return now(clock);
}

/* glibc's name, which ps and top show: at most 15 bytes */
void
tsetname(char *name)
{
	char buf[16];

	strncpy(buf, name, sizeof buf - 1);
	buf[sizeof buf - 1] = 0;
	pthread_setname_np(pthread_self(), buf);
}
