/*
 * The plan9port part of the sound and music drivers' shim (p9audio.h),
 * for the fast loop: there is no /dev/audio, so with $p9audio set the
 * sound is written to that file as raw PCM (16-bit stereo), paced as a
 * device would take it: the game may run a fixed amount ahead of the
 * clock.  No music.
 */
#include <u.h>
#include <libc.h>
#include "p9audio.h"

enum
{
	Ahead	= 8192,		/* frames ahead of the clock */
};

static int	fd = -1;
static int	rate;
static vlong	t0;
static vlong	written;

int
p9audio_open(int r)
{
	char *f;

	f = getenv("p9audio");
	if(f == nil)
		return -1;
	fd = create(f, OWRITE, 0644);
	free(f);
	if(fd < 0)
		return -1;
	rate = r;
	t0 = nsec();
	written = 0;
	return 0;
}

int
p9audio_space(void)
{
	vlong played, n;

	if(fd < 0)
		return 0;
	played = (nsec() - t0) * rate / 1000000000LL;
	n = played + Ahead - written;
	if(n < 0)
		return 0;
	return n > Ahead ? Ahead : n;
}

void
p9audio_write(short *buf, int frames)
{
	if(fd < 0)
		return;
	if(write(fd, buf, frames*4) == frames*4)
		written += frames;
}

void
p9audio_close(void)
{
	if(fd >= 0)
		close(fd);
	fd = -1;
}

int
p9music_play(char*, char*, int)
{
	return -1;
}

void
p9music_stop(void)
{
}

int
p9music_playing(void)
{
	return 0;
}

int
p9music_read(short *buf, int frames)
{
	memset(buf, 0, frames*4);
	return 0;
}
