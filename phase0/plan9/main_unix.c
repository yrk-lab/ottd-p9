/*
 * main for separately translated units (../tu) built on Linux with
 * the system's C library (mk SYS=linux): lower.py --libc renamed the
 * program's main c_main, and its static constructors are in
 * __p9_ctors (ctors.c), run first, as libc_p9.c's main does on
 * Plan 9.
 */
#include <stdlib.h>

int	c_main(int, char**);
void	__p9_ctors(void);

int
main(int argc, char **argv)
{
	__p9_ctors();
	exit(c_main(argc, argv));
}
