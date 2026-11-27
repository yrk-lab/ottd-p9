/*
 * os_p9.c's calls that goken's Linux-hosted libc has already, as
 * system calls of its own: linked on 9front only.
 */
#include <u.h>
#include <libc.h>

int	c_open(char*, int, ...);	/* os_p9.c */
int	*__errno_location(void);	/* libc_p9.c */

enum
{
	Ocreat		= 0100,	/* Linux open(2) */
	Enosys		= 38,
	Atfdcwd		= -100,
};

/* AT_FDCWD only, as os_p9.c's unlinkat */
int
openat(int dirfd, char *path, int flags, ...)
{
	va_list arg;
	int perm;

	if(dirfd != Atfdcwd){
		*__errno_location() = Enosys;
		return -1;
	}
	perm = 0666;
	if(flags & Ocreat){
		va_start(arg, flags);
		perm = va_arg(arg, int);
		va_end(arg);
	}
	return c_open(path, flags, perm);
}
