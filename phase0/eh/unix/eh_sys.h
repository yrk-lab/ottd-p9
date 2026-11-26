/* eh_rt for Unix (the gcc stand-in): ISO C library */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#define	eh_puts(s)	fputs((s), stderr)
#define	eh_flush()	fflush(stdout)
