/*
 * The program's atomics on Plan 9 (9front and 9legacy alike): thread.h's
 * tcas and tadd, and the __atomic_* calls cbe makes of atomic
 * instructions, on a system's compare and swap (atom9f.c: 9front's
 * atomic(2); atom9l.c: 9legacy's cas32 and casp).
 */
#include <u.h>
#include <libc.h>
#include "thread.h"

/* atom9f.c, atom9l.c: 1 if *p was old and is now new */
int	atcas32(u32int *p, u32int old, u32int new);
int	atcas64(u64int *p, u64int old, u64int new);

static u32int
atadd32(u32int *p, u32int v)
{
	u32int o;

	do
		o = *(volatile u32int*)p;
	while(!atcas32(p, o, o + v));
	return o + v;
}

/* aligned 64-bit loads are atomic on amd64 */
static u64int
atget64(u64int *p)
{
	return *(volatile u64int*)p;
}

static u64int
atadd64(u64int *p, u64int v)
{
	u64int o;

	do
		o = atget64(p);
	while(!atcas64(p, o, o + v));
	return o + v;
}

static u32int
atswap32(u32int *p, u32int v)
{
	u32int o;

	do
		o = *(volatile u32int*)p;
	while(!atcas32(p, o, v));
	return o;
}

static u64int
atswap64(u64int *p, u64int v)
{
	u64int o;

	do
		o = atget64(p);
	while(!atcas64(p, o, v));
	return o;
}

int
tcas(int *p, int old, int new)
{
	return atcas32((u32int*)p, old, new);
}

int
tadd(int *p, int v)
{
	return atadd32((u32int*)p, v);
}

/*
 * __atomic_* library calls, as cbe makes them of atomic instructions,
 * on the system's compare and swap, which is sequentially consistent:
 * the memory orders (the last arguments) are all served by it.
 * Aligned loads are atomic on amd64.  A byte is changed by a compare and swap of the
 * aligned word around it.
 */

uint
__atomic_fetch_add_4(uint *p, uint v, int)
{
	return atadd32(p, v) - v;
}

uint
__atomic_fetch_sub_4(uint *p, uint v, int)
{
	return atadd32(p, -v) + v;
}

uvlong
__atomic_fetch_add_8(uvlong *p, uvlong v, int)
{
	return atadd64(p, v) - v;
}

uvlong
__atomic_fetch_sub_8(uvlong *p, uvlong v, int)
{
	return atadd64(p, -v) + v;
}

uchar
__atomic_load_1(uchar *p, int)
{
	return *(volatile uchar*)p;
}

uint
__atomic_load_4(uint *p, int)
{
	return *(volatile uint*)p;
}

uvlong
__atomic_load_8(uvlong *p, int)
{
	return atget64(p);
}

void
__atomic_store_4(uint *p, uint v, int)
{
	atswap32(p, v);
}

void
__atomic_store_8(uvlong *p, uvlong v, int)
{
	atswap64(p, v);
}

uint
__atomic_exchange_4(uint *p, uint v, int)
{
	return atswap32(p, v);
}

uvlong
__atomic_exchange_8(uvlong *p, uvlong v, int)
{
	return atswap64(p, v);
}

int
__atomic_compare_exchange_4(uint *p, uint *expected, uint desired, int, int)
{
	uint v;

	for(;;){
		if(atcas32(p, *expected, desired))
			return 1;
		v = *(volatile uint*)p;
		if(v != *expected){
			*expected = v;
			return 0;
		}
	}
}

int
__atomic_compare_exchange_8(uvlong *p, uvlong *expected, uvlong desired, int, int)
{
	uvlong v;

	for(;;){
		if(atcas64(p, *expected, desired))
			return 1;
		v = atget64(p);
		if(v != *expected){
			*expected = v;
			return 0;
		}
	}
}

/* op (0 store, 1 and, 2 or) v into the byte at p; the old byte */
static uchar
byteop(uchar *p, uchar v, int op)
{
	u32int *w;
	int shift;
	u32int o, n;
	uchar b, nb;

	w = (u32int*)((uintptr)p & ~3);
	shift = ((uintptr)p & 3) * 8;
	do{
		o = *(volatile u32int*)w;
		b = o >> shift;
		switch(op){
		default:
			nb = v;
			break;
		case 1:
			nb = b & v;
			break;
		case 2:
			nb = b | v;
			break;
		}
		n = (o & ~(0xFF << shift)) | (u32int)nb << shift;
	}while(!atcas32(w, o, n));
	return b;
}

uchar
__atomic_exchange_1(uchar *p, uchar v, int)
{
	return byteop(p, v, 0);
}

uchar
__atomic_fetch_and_1(uchar *p, uchar v, int)
{
	return byteop(p, v, 1);
}

uchar
__atomic_fetch_or_1(uchar *p, uchar v, int)
{
	return byteop(p, v, 2);
}

void
__atomic_store_1(uchar *p, uchar v, int)
{
	byteop(p, v, 0);
}

