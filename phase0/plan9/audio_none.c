/*
 * The sound and music drivers' shim (p9audio.h) where there is no
 * audio: goken and the native Linux builds.  Opening fails, so the
 * game picks another driver.  No includes: the native builds compile
 * it with the system's compiler.
 */

int
p9audio_open(int rate)
{
	(void)rate;
	return -1;
}

int
p9audio_space(void)
{
	return 0;
}

void
p9audio_write(short *buf, int frames)
{
	(void)buf;
	(void)frames;
}

void
p9audio_close(void)
{
}

int
p9music_play(char *file, char *bank, int rate)
{
	(void)file;
	(void)bank;
	(void)rate;
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
	int i;

	for(i = 0; i < 2*frames; i++)
		buf[i] = 0;
	return 0;
}
