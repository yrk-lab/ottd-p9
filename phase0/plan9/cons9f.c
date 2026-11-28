/*
 * The dedicated server's console on 9front: OpenTTD polls standard
 * input with select and then reads a line through std::cin.  A read
 * of fd 0 would block the game, and Plan 9 cannot ask whether one
 * would: as for the sockets, a helper process (thread9f.c's p9proc,
 * ended with the program) reads it into a ring in shared memory, from
 * the first time select is asked about it on.  Then select's answer
 * is the ring's (stdinready, net_p9.c), and stdin's buffer is filled
 * from the ring (c_stdinread, libc_p9.c's cgetc), not from fd 0.
 */
#include <u.h>
#include <libc.h>

enum
{
	Nring	= 8192,
};

int	p9proc(void (*)(void*), void*);	/* thread9f.c */
void	tsetname(char*);
extern long	(*c_stdinread)(void*, long);	/* libc_p9.c */

static Lock	lk;
static uchar	ring[Nring];
static int	r, cnt;
static int	eof;
static int	started;

static void
reader(void*)
{
	uchar buf[512];
	int n, i, room;

	tsetname("cons");
	for(;;){
		n = read(0, buf, sizeof buf);
		if(n <= 0)
			break;
		for(i = 0; i < n; ){
			lock(&lk);
			room = Nring - cnt;
			while(room > 0 && i < n){
				ring[(r + cnt) % Nring] = buf[i++];
				cnt++;
				room--;
			}
			unlock(&lk);
			if(i < n)
				sleep(10);	/* full: the game takes it a line a tick */
		}
	}
	lock(&lk);
	eof = 1;
	unlock(&lk);
}

/* like read(0, ...): what the ring has, waiting for some; 0 at the end */
static long
ringread(void *a, long n)
{
	uchar *p;
	long m;

	p = a;
	for(;;){
		lock(&lk);
		if(cnt > 0 || eof)
			break;
		unlock(&lk);
		sleep(10);
	}
	for(m = 0; m < n && cnt > 0; m++){
		p[m] = ring[r];
		r = (r + 1) % Nring;
		cnt--;
	}
	unlock(&lk);
	return m;
}

/* would a read of standard input not block?  The first call starts the reader */
int
stdinready(void)
{
	int y;

	if(!started){
		started = 1;
		c_stdinread = ringread;
		if(p9proc(reader, nil) < 0){
			c_stdinread = nil;
			return 0;
		}
	}
	lock(&lk);
	y = cnt > 0 || eof;
	unlock(&lk);
	return y;
}
