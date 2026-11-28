/*
 * 9legacy's compare and swap for atom_p9.c: its libc's cas32, and casp
 * for 64 bits (a pointer's size on amd64: CMPXCHGQ).  9legacy has no
 * atomic(2).
 */
#include <u.h>
#include <libc.h>

int
atcas32(u32int *p, u32int old, u32int new)
{
	return cas32(p, old, new);
}

int
atcas64(u64int *p, u64int old, u64int new)
{
	return casp((void**)p, (void*)old, (void*)new);
}
