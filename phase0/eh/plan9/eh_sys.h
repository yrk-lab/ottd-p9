/* eh_rt for Plan 9: setjmp, longjmp, calloc, abort are in libc */
#include <u.h>
#include <libc.h>

#define	NULL	nil

/* the type id table is shared by the threads (thread9f.c) */
#define	eh_lock()	lock(&eh_lk)
#define	eh_unlock()	unlock(&eh_lk)
static Lock	eh_lk;

int	c_fflush(void*);	/* libc_p9.c: stdout is buffered there */
/* no string literal in a macro body: 9front's cpp garbles string arguments then */
#define	eh_puts(s)	write(2, (s), strlen(s))
#define	eh_flush()	c_fflush(nil)
