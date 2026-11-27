/*
 * What thread_p9.c, the program's threads (its pthread calls, renamed
 * c_pthread_* by lower.py), needs of its system part: thread9f.c on
 * 9front (processes sharing memory), threadp9p.c with plan9port
 * (glibc's threads, for the fast loop).
 */

typedef struct Thread Thread;

/* the calling thread's (nil: the main thread, before any other) */
Thread	*tself(void);

/* run fn(t) in a new thread, whose tself is t; fn returns when it is done */
int	tstart(Thread *t, void (*fn)(Thread*));

/* a counting semaphore in an int: up n; down one, waiting up to ms (<0: no end): 1, or 0 when the time ran out */
void	semup(int *s, int n);
int	semdown(int *s, vlong ms);

/* atomic: compare and swap (1 if swapped), add (the new value) */
int	tcas(int *p, int old, int new);
int	tadd(int *p, int v);

/* let another thread run */
void	tyield(void);

/* the time on a clock the program names (CLOCK_REALTIME 0, CLOCK_MONOTONIC 1), in nanoseconds */
vlong	tnow(int clock);
