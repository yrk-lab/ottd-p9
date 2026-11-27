/*
 * Itanium C++ ABI exception entry points (__cxa_*) and the state
 * behind the Emscripten-style lowering; see eh_rt.h.
 *
 * An exception object is malloc'd with an Exc header in front.  One
 * exception at a time is in flight (thrown); caught ones form a stack
 * (caught), as handlers can nest.  Matching a catch clause against the
 * thrown type is done by libc++abi's typeinfo code (eh_can_catch in
 * eh_match.cpp, translated with the program).
 */
#include "eh_sys.h"	/* unix/ or plan9/ */
#include "eh_rt.h"

typedef struct Exc Exc;
struct Exc
{
	void	*type;		/* std::type_info* of the thrown object */
	void	(*dtor)(void*);
	void	*adjusted;	/* object pointer for the matched catch clause */
	int	handlers;	/* catch clauses currently inside this exception */
	int	rethrown;
	Exc	*next;		/* caught stack */
};

enum
{
	Hdr	= (sizeof(Exc) + 15) & ~15,	/* objects stay 16-byte aligned */
	Ntypes	= 1024,
};

#ifdef EH_TLS
#define	thrown		(*(Exc**)&eh_self()->xthrown)
#define	caught		(*(Exc**)&eh_self()->xcaught)
#define	tempret0	(eh_self()->xtempret0)
#else
Ehframe		*eh_top;
long long	__THREW__;
static Exc	*thrown;
static Exc	*caught;
static int	tempret0;
#endif

/* shared by the threads: eh_lock (eh_sys.h) */
static void	*typeids[Ntypes];
static int	ntypeids;

int	eh_can_catch(void *catchtype, void *throwntype, void **adjusted);

static Exc*
hdr(void *obj)
{
	return (Exc*)((char*)obj - Hdr);
}

static void*
obj(Exc *e)
{
	return (char*)e + Hdr;
}

/* flush stdout, say why (and what), abort */
static void
terminate2(char *why, char *what)
{
	eh_flush();
	eh_puts("terminate: ");
	eh_puts(why);
	if(what != NULL){
		eh_puts(" ");
		eh_puts(what);
	}
	eh_puts("\n");
	abort();
}

static void
terminate(char *why)
{
	terminate2(why, NULL);
}

/* resume at the innermost __invoke_ wrapper */
static void
unwind(void)
{
	char **ti;

	if(eh_top == NULL){
		/* std::type_info is { vtable, mangled name } */
		ti = thrown != NULL ? (char**)thrown->type : NULL;
		terminate2("uncaught exception of type", ti != NULL ? ti[1] : "?");
	}
	longjmp(eh_top->jb, 1);
}

static void
destroy(Exc *e)
{
	if(e->dtor != NULL)
		e->dtor(obj(e));
	free(e);
}

void*
__cxa_allocate_exception(unsigned long long size)
{
	Exc *e;

	e = calloc(1, Hdr + size);
	if(e == NULL)
		terminate("out of memory allocating an exception");
	return obj(e);
}

void
__cxa_free_exception(void *o)
{
	free(hdr(o));
}

void
__cxa_throw(void *o, void *type, void (*dtor)(void*))
{
	Exc *e;

	e = hdr(o);
	e->type = type;
	e->dtor = dtor;
	thrown = e;
	unwind();
}

void
__cxa_rethrow(void)
{
	if(caught == NULL)
		terminate("rethrow with no exception");
	caught->rethrown = 1;
	thrown = caught;
	unwind();
}

void
__resumeException(void *o)
{
	thrown = hdr(o);
	unwind();
}

/* the id a landing pad compares the selector with */
int
llvm_eh_typeid_for(void *type)
{
	int i;

	eh_lock();
	for(i = 0; i < ntypeids; i++)
		if(typeids[i] == type){
			eh_unlock();
			return i+1;
		}
	if(ntypeids == Ntypes)
		terminate("too many exception types");
	typeids[ntypeids++] = type;
	i = ntypeids;
	eh_unlock();
	return i;
}

int
getTempRet0(void)
{
	return tempret0;
}

void
setTempRet0(int v)
{
	tempret0 = v;
}

/*
 * Landing pad: find the first catch clause (a null type is catch(...))
 * that matches the exception in flight.  The selector is that clause's
 * type id, or 0 for a cleanup.  Returns the exception object.
 */
void*
eh_match(void **types, int n)
{
	Exc *e;
	void *adj;
	int i;

	e = thrown;
	if(e == NULL)
		terminate("landing pad with no exception");
	tempret0 = 0;
	e->adjusted = obj(e);
	for(i = 0; i < n; i++){
		adj = obj(e);
		if(types[i] == NULL || eh_can_catch(types[i], e->type, &adj)){
			e->adjusted = adj;
			tempret0 = llvm_eh_typeid_for(types[i]);
			break;
		}
	}
	return obj(e);
}

void*
__cxa_begin_catch(void *o)
{
	Exc *e;

	e = hdr(o);
	if(e != caught){
		e->next = caught;
		caught = e;
	}
	e->handlers++;
	e->rethrown = 0;
	if(thrown == e)
		thrown = NULL;
	return e->adjusted;
}

void
__cxa_end_catch(void)
{
	Exc *e;

	e = caught;
	if(e == NULL)
		return;
	if(--e->handlers > 0)
		return;
	caught = e->next;
	if(!e->rethrown)
		destroy(e);
}

void*
__cxa_get_exception_ptr(void *o)
{
	return hdr(o)->adjusted;
}

unsigned int
__cxa_uncaught_exceptions(void)
{
	return thrown != NULL;
}

/* std::uncaught_exception() */
int
_ZSt18uncaught_exceptionv(void)
{
	return thrown != NULL;
}

void*
__cxa_current_exception_type(void)
{
	if(caught == NULL)
		return NULL;
	return caught->type;
}

/* std::terminate() */
void
_ZSt9terminatev(void)
{
	terminate("std::terminate");
}

int
__gxx_personality_v0(void)
{
	terminate("personality routine called");
	return 0;
}
