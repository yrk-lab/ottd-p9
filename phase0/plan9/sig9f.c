/*
 * signal() on 9front: the notes a program can be sent are its
 * signals.  An interrupt note (the Del key, as rio and the console
 * send it, or written to /proc/n/note) is SIGINT, a hangup note
 * SIGHUP; a handler the program set runs from the note handler
 * (OpenTTD's set a flag: the dedicated server quits, the Plan 9 video
 * driver asks to quit).  SIG_IGN ignores the note; with neither, or
 * for any other note (kill among them), the note does what it always
 * does.  The note handler is every process's: rfork passes it on to
 * the threads and helpers.
 */
#include <u.h>
#include <libc.h>

enum
{
	Sighup	= 1,
	Sigint	= 2,
	Nsig	= 65,
};

#define	SIGDFL	((void(*)(int))0)
#define	SIGIGN	((void(*)(int))1)

static void	(*handler[Nsig])(int);
static int	installed;

static int
notesig(void*, char *note)
{
	int sig;
	void (*f)(int);

	if(strcmp(note, "interrupt") == 0)
		sig = Sigint;
	else if(strncmp(note, "hangup", 6) == 0)
		sig = Sighup;
	else
		return 0;
	f = handler[sig];
	if(f == SIGDFL)
		return 0;
	if(f != SIGIGN)
		f(sig);
	return 1;
}

void*
signal(int sig, void (*f)(int))
{
	void (*old)(int);

	if(sig <= 0 || sig >= Nsig)
		return (void*)-1;	/* SIG_ERR */
	old = handler[sig];
	handler[sig] = f;
	if(!installed && f != SIGDFL){
		atnotify(notesig, 1);
		installed = 1;
	}
	return (void*)old;
}
