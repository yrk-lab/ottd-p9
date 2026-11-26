/* <setjmp.h> for translated code: Plan 9's own setjmp and longjmp (libc).
 * The jmp_buf storage the program reserves is glibc's (200 bytes), larger
 * than Plan 9's, so casting it to this type is safe. */
typedef unsigned long long	jmp_buf[2];
int	setjmp(jmp_buf);
void	longjmp(jmp_buf, int);
