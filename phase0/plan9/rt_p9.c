/* Plan 9 C++ runtime shim (probe): same role as ../rt.c but written against Plan 9 libc. */
#include <u.h>
#include <libc.h>
void *_Znwm(uvlong n)            { void *p = malloc(n ? n : 1); if(p == nil) abort(); return p; }
void *_Znam(uvlong n)            { return _Znwm(n); }
void  _ZdlPv(void *p)            { free(p); }
void  _ZdaPv(void *p)            { free(p); }
void  _ZdlPvm(void *p, uvlong n) { USED(n); free(p); }
void  _ZdaPvm(void *p, uvlong n) { USED(n); free(p); }
void  __cxa_pure_virtual(void)   { fprint(2, "pure virtual call\n"); abort(); }
int   __cxa_atexit(void (*f)(void*), void *a, void *d) { USED(f); USED(a); USED(d); return 0; }
int   bcmp(void *a, void *b, uvlong n) { return memcmp(a, b, n); }   /* LLVM rewrites memcmp()==0 as bcmp */
float ceilf(float x)                   { return (float)ceil((double)x); }
void  _ZNSt3__122__libcpp_verbose_abortEPKcz(char *fmt, ...) { USED(fmt); fprint(2, "libc++ abort\n"); abort(); }
