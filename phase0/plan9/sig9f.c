/*
 * signal() on 9front: the notes a program can be sent are its
 * signals.  An interrupt note (the Del key, as rio and the console
 * send it, or written to /proc/n/note, maybe with a newline) is
 * SIGINT, a hangup note SIGHUP, a write on a closed pipe SIGPIPE; a
 * handler the program set runs from the note handler (OpenTTD's set
 * a flag: the dedicated server quits, the Plan 9 video driver asks to
 * quit), and SIG_IGN ignores the note.  With neither, or for any
 * other note (kill among them), the note does what it always does,
 * and the program's other processes are ended first (thread9f.c's
 * p9killkin), as libthread ends them all on a note nobody takes; the
 * note they are sent (Kinnote) ends each quietly.  The handler is set
 * before any fork (noteinit, from p9runmain): every process has it.
 */
#include <u.h>
#include <libc.h>

enum
{
	Sighup	= 1,
	Sigint	= 2,
	Sigpipe	= 13,
	Nsig	= 65,
};

#define	SIGDFL	((void(*)(int))0)
#define	SIGIGN	((void(*)(int))1)

static void	(*handler[Nsig])(int);
static int	installed;

extern char	Kinnote[];		/* thread9f.c */
void	p9killkin(void);
void	noteinit(void);

/* note is name, or name and a newline */
static int
noteis(char *note, char *name)
{
	int n;

	n = strlen(name);
	return strncmp(note, name, n) == 0 && (note[n] == 0 || strcmp(note+n, "\n") == 0);
}

static int
notesig(void*, char *note)
{
	int sig;
	void (*f)(int);

	if(noteis(note, Kinnote))
		return 0;
	if(noteis(note, "interrupt"))
		sig = Sigint;
	else if(strncmp(note, "hangup", 6) == 0)
		sig = Sighup;
	else if(strncmp(note, "sys: write on closed pipe", 25) == 0)
		sig = Sigpipe;
	else{
		p9killkin();
		return 0;
	}
	f = handler[sig];
	if(f == SIGDFL){
		p9killkin();
		return 0;
	}
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
	noteinit();
	return (void*)old;
}

void
noteinit(void)
{
	if(!installed){
		atnotify(notesig, 1);
		installed = 1;
	}
}
