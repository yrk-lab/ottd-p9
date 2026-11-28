/*
 * 9front's compare and swap for atom_p9.c: atomic(2).
 */
#include <u.h>
#include <libc.h>

int
atcas32(u32int *p, u32int old, u32int new)
{
	return acasl((Along*)p, old, new);
}

int
atcas64(u64int *p, u64int old, u64int new)
{
	return acasv((Avlong*)p, old, new);
}
