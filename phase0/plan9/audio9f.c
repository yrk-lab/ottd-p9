/*
 * The 9front part of the sound and music drivers' shim (p9audio.h).
 *
 * Sound: the game mixes in its own process, a little ahead, into a
 * ring; a helper process (sharing memory: thread9f.c's p9proc) writes the ring
 * to /dev/audio, whose writes block at the device's pace, so the game
 * never waits on the device.  The game's mixer must stay in the game's
 * process: the runtime's mutexes are no-ops.
 *
 * Music: games/dmid turns the MIDI file into OPL3 instructions and
 * games/opl3 plays them as PCM (resampled by its own pcmconv).  dmid's
 * instrument bank is the one the driver found among the game's data,
 * else 9front's own: the GENMIDI lump of its shareware Doom WAD, which
 * games/wadfs serves in the pipeline's namespace (rfork n), as dmid(1)
 * does it.  The rc that starts it execs opl3, which reads dmid through
 * <{}: no shell is left waiting.  A helper reads the pipeline's output into
 * a second ring, which the mixer takes as its music source, so one
 * writer has /dev/audio.  The ring being full holds the pipeline back:
 * it makes sound as fast as it is played.
 */
#include <u.h>
#include <libc.h>
#include "p9audio.h"

enum
{
	Nring	= 8192,		/* frames ahead: 186 ms at 44100 Hz */
	Nmusic	= 16384,
	Chunk	= 1024,		/* frames a write or read */
	Rate	= 44100,	/* /dev/audio's and opl3's */
};

typedef struct Ring Ring;
struct Ring
{
	Lock;
	ulong	*f;		/* frames: two 16-bit samples */
	int	n;
	int	r;
	int	cnt;
	int	fd;		/* /dev/audio, or the music pipeline */
	int	eof;		/* music: the pipeline ended */
	int	dead;		/* stop, helper */
	int	done;		/* the helper has stopped */
	void	(*run)(Ring*);	/* the helper */
};

static Ring	out;
static Ring	music;
static int	musicpid;	/* the pipeline's note group */

static int
put(Ring *g, ulong *f, int n)
{
	int i, m;

	lock(g);
	m = g->n - g->cnt;
	if(n > m)
		n = m;
	for(i = 0; i < n; i++)
		g->f[(g->r + g->cnt + i) % g->n] = f[i];
	g->cnt += n;
	unlock(g);
	return n;
}

static int
take(Ring *g, ulong *f, int n)
{
	int i;

	lock(g);
	if(n > g->cnt)
		n = g->cnt;
	for(i = 0; i < n; i++)
		f[i] = g->f[(g->r + i) % g->n];
	g->r = (g->r + n) % g->n;
	g->cnt -= n;
	unlock(g);
	return n;
}

static int
dead(Ring *g)
{
	int d;

	lock(g);
	d = g->dead;
	unlock(g);
	return d;
}

static void
finish(Ring *g)
{
	lock(g);
	g->done = 1;
	unlock(g);
}

int	p9proc(void (*)(void*), void*);	/* thread9f.c */
void	tsetname(char*);

static void
ringrun(void *a)
{
	Ring *g;

	g = a;
	g->run(g);
}

/* start fn in a process sharing memory (thread9f.c's p9proc: on a stack in it) */
static int
helper(void (*fn)(Ring*), Ring *g)
{
	g->dead = 0;
	g->done = 0;
	g->run = fn;
	return p9proc(ringrun, g);
}

/* stop a ring's helper, waiting a moment for it */
static void
stophelper(Ring *g)
{
	int i;

	lock(g);
	g->dead = 1;
	unlock(g);
	if(g->fd >= 0)
		close(g->fd);
	g->fd = -1;
	for(i = 0; i < 100; i++){
		lock(g);
		if(g->done){
			unlock(g);
			break;
		}
		unlock(g);
		sleep(10);
	}
}

/*
 * Sound
 */

static void
writer(Ring *g)
{
	ulong buf[Chunk];
	int n;

	tsetname("audio");
	while(!dead(g)){
		n = take(g, buf, Chunk);
		if(n == 0){
			sleep(5);
			continue;
		}
		if(write(g->fd, buf, n*4) != n*4)
			break;
	}
	finish(g);
}

int
p9audio_open(int rate)
{
	if(rate != Rate)
		return -1;
	out.fd = open("/dev/audio", OWRITE);
	if(out.fd < 0)
		return -1;
	out.f = mallocz(Nring * sizeof out.f[0], 1);
	if(out.f == nil){
		close(out.fd);
		return -1;
	}
	out.n = Nring;
	out.r = out.cnt = 0;
	if(helper(writer, &out) < 0){
		close(out.fd);
		return -1;
	}
	return 0;
}

int
p9audio_space(void)
{
	int n;

	lock(&out);
	n = out.n - out.cnt;
	unlock(&out);
	return n;
}

void
p9audio_write(short *buf, int frames)
{
	put(&out, (ulong*)buf, frames);
}

void
p9audio_close(void)
{
	p9music_stop();
	stophelper(&out);
}

/*
 * Music
 */

static void
reader(Ring *g)
{
	ulong buf[Chunk];
	int n, m, o;

	tsetname("music");
	while(!dead(g)){
		n = read(g->fd, buf, sizeof buf);
		if(n <= 0)
			break;
		n /= 4;		/* a frame split between reads is dropped: opl3 writes whole ones */
		for(o = 0; o < n && !dead(g); o += m){
			m = put(g, buf + o, n - o);
			if(m == 0)
				sleep(5);	/* full: as fast as it is played */
		}
	}
	lock(g);
	g->eof = 1;
	unlock(g);
	finish(g);
}

/* s, quoted for rc */
static char*
rcquote(char *s)
{
	char *q, *p;

	q = malloc(2*strlen(s) + 3);
	if(q == nil)
		return nil;
	p = q;
	*p++ = '\'';
	for(; *s != 0; s++){
		if(*s == '\'')
			*p++ = '\'';
		*p++ = *s;
	}
	*p++ = '\'';
	*p = 0;
	return q;
}

int
p9music_play(char *file, char *bank, int rate)
{
	int p[2], pid, fd;
	char *qf, *qb, *cmd;

	p9music_stop();
	if(rate != Rate)
		return -1;
	if(music.f == nil){
		music.f = mallocz(Nmusic * sizeof music.f[0], 1);
		if(music.f == nil)
			return -1;
		music.n = Nmusic;
	}
	qf = rcquote(file);
	if(bank != nil && bank[0] != 0){
		qb = rcquote(bank);
		cmd = smprint("exec games/opl3 <{games/dmid -i %s %s}", qb, qf);
	}else{
		qb = nil;
		cmd = smprint("rfork n; games/wadfs -r /sys/games/lib/doom/doom1.wad >[2]/dev/null; "
			"exec games/opl3 <{games/dmid -i /mnt/wad/genmidi %s}", qf);
	}
	free(qb);
	free(qf);
	if(cmd == nil || pipe(p) < 0){
		free(cmd);
		return -1;
	}
	pid = rfork(RFPROC|RFFDG|RFNOTEG|RFNOWAIT);
	if(pid < 0){
		close(p[0]);
		close(p[1]);
		free(cmd);
		return -1;
	}
	if(pid == 0){
		fd = open("/dev/null", OREAD);
		dup(fd, 0);
		dup(p[1], 1);
		close(p[0]);
		close(p[1]);
		execl("/bin/rc", "rc", "-c", cmd, nil);
		_exits("exec");
	}
	free(cmd);
	close(p[1]);
	musicpid = pid;
	music.fd = p[0];
	music.r = music.cnt = 0;
	music.eof = 0;
	if(helper(reader, &music) < 0){
		p9music_stop();
		return -1;
	}
	return 0;
}

void
p9music_stop(void)
{
	if(musicpid == 0)
		return;
	postnote(PNGROUP, musicpid, "kill");
	musicpid = 0;
	stophelper(&music);
	lock(&music);
	music.cnt = 0;
	unlock(&music);
}

/*
 * Music needs the pipeline's programs: 9front has them, 9legacy (4th
 * edition) has no games/dmid or games/opl3; without them the game takes
 * its null music driver
 */
int
p9music_open(void)
{
	if(access("/bin/games/dmid", AEXEC) < 0 || access("/bin/games/opl3", AEXEC) < 0)
		return -1;
	return 0;
}

int
p9music_playing(void)
{
	int r;

	if(musicpid == 0)
		return 0;
	lock(&music);
	r = !music.eof || music.cnt > 0;
	unlock(&music);
	return r;
}

int
p9music_read(short *buf, int frames)
{
	int n;

	if(musicpid == 0)
		n = 0;
	else
		n = take(&music, (ulong*)buf, frames);
	memset(buf + 2*n, 0, (frames - n) * 4);
	return n;
}
