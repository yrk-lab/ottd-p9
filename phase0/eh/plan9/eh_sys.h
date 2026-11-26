/* eh_rt for Plan 9: setjmp, longjmp, calloc, abort are in libc */
#include <u.h>
#include <libc.h>

#define	NULL	nil
#define	eh_die(s)	(fprint(2, "terminate: %s\n", (s)), abort())
