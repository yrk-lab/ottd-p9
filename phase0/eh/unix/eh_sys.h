/* eh_rt for Unix (the gcc stand-in): ISO C library */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#define	eh_die(s)	(fprintf(stderr, "terminate: %s\n", (s)), abort())
