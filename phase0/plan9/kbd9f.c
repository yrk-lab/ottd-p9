/*
 * The 9front keyboard of the draw shim: /dev/kbd, which tells keys
 * down and up as well as characters typed.  Its messages, each ending
 * in a NUL:
 *	k runes		the keys now down, after a key went down
 *	K runes		the keys now down, after a key went up
 *	c rune		a character typed (repeated while held)
 * Ups and downs of the held set become P9Ekeydown and P9Ekeyup, the
 * characters P9Echar.  See p9draw.h.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <event.h>
#include "p9draw.h"

enum
{
	Nheld	= 32,
	Nq	= 128,
};

static ulong	kbdkey;		/* the event key of /dev/kbd, or 0 */
static Rune	held[Nheld];
static int	nheld;
static P9Event	q[Nq];		/* events parsed, not yet taken */
static int	q0, nq;

ulong
kbdevents(void)
{
	return 0;	/* /dev/cons is not read */
}

void
kbdinit(void)
{
	int fd;

	fd = open("/dev/kbd", OREAD);
	if(fd >= 0)
		kbdkey = estart(0, fd, 256);
}

static void
put(int type, Rune r)
{
	P9Event *e;

	if(nq == Nq)
		return;
	e = &q[(q0 + nq++) % Nq];
	memset(e, 0, sizeof *e);
	e->type = type;
	e->rune = r;
}

static int
isheld(Rune *set, int n, Rune r)
{
	int i;

	for(i = 0; i < n; i++)
		if(set[i] == r)
			return 1;
	return 0;
}

/* a k or K message: the new held set */
static void
newheld(char *s)
{
	Rune now[Nheld];
	int n, i;

	for(n = 0; *s != 0 && n < Nheld; n++)
		s += chartorune(&now[n], s);
	for(i = 0; i < n; i++)
		if(!isheld(held, nheld, now[i]))
			put(P9Ekeydown, now[i]);
	for(i = 0; i < nheld; i++)
		if(!isheld(now, n, held[i]))
			put(P9Ekeyup, held[i]);
	memmove(held, now, n * sizeof(Rune));
	nheld = n;
}

static void
parse(char *buf, int n)
{
	char *p, *e;
	Rune r;

	buf[n] = 0;
	for(p = buf; p < buf + n; p = e + 1){
		e = p + strlen(p);
		switch(*p){
		case 'k':
		case 'K':
			newheld(p + 1);
			break;
		case 'c':
			chartorune(&r, p + 1);
			put(P9Echar, r);
			break;
		}
	}
}

int
kbdevent(P9Event *e)
{
	Event ev;
	char buf[EMAXMSG+1];
	int n;

	if(nq == 0 && kbdkey != 0 && ecanread(kbdkey)){
		eread(kbdkey, &ev);
		n = ev.n;
		if(n > EMAXMSG)
			n = EMAXMSG;
		memmove(buf, ev.data, n);
		parse(buf, n);
	}
	if(nq == 0)
		return 0;
	*e = q[q0];
	q0 = (q0 + 1) % Nq;
	nq--;
	return 1;
}
