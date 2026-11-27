/*
 * The shim under OpenTTD's Plan 9 sound and music drivers
 * (src/sound/plan9_s.cpp, src/music/plan9_m.cpp, patches/plan9-audio.patch
 * in phase1): ints and pointers only, so the translated C++ and the
 * native C agree without a struct between them.  Samples are 16-bit
 * signed stereo frames, as OpenTTD's mixer makes them.
 *	audio9f.c	9front: /dev/audio; music by games/dmid | games/opl3
 *	audiop9p.c	plan9port: the sound to a file ($p9audio), no music
 *	audio_none.c	no audio (goken, the native Linux builds)
 */

/* sound: open at rate frames a second; frames that can be written now; write them */
int	p9audio_open(int rate);
int	p9audio_space(void);
void	p9audio_write(short *buf, int frames);
void	p9audio_close(void);

/*
 * music: play a standard MIDI file, with the instrument bank given ("":
 * the system's); whether it still plays; its next frames (the rest
 * silence)
 */
int	p9music_play(char *file, char *bank, int rate);
void	p9music_stop(void);
int	p9music_playing(void);
int	p9music_read(short *buf, int frames);
