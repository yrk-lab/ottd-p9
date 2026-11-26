/* Minimal C++ runtime shim for translated code (Phase 0).
 * Symbols use the Itanium-mangled names the translated C refers to.
 * With -fno-exceptions -fno-rtti nothing else is needed for plain classes/STL. */
#include <stdlib.h>
#include <stdio.h>
void *_Znwm(unsigned long n)            { void *p = malloc(n ? n : 1); if (!p) abort(); return p; } /* operator new(size_t)   */
void *_Znam(unsigned long n)            { return _Znwm(n); }                                      /* operator new[](size_t) */
void  _ZdlPv(void *p)                   { free(p); }                                              /* operator delete(void*) */
void  _ZdaPv(void *p)                   { free(p); }                                              /* operator delete[](void*)*/
void  _ZdlPvm(void *p, unsigned long n) { (void)n; free(p); }                                     /* sized delete           */
void  _ZdaPvm(void *p, unsigned long n) { (void)n; free(p); }
void  __cxa_pure_virtual(void)          { fputs("pure virtual call\n", stderr); abort(); }
int   __cxa_atexit(void (*f)(void *), void *a, void *d) { (void)f; (void)a; (void)d; return 0; }
/* libc++ hardening/verbose-abort hook (called on assertion/throw paths when exceptions are off) */
void _ZNSt3__122__libcpp_verbose_abortEPKcz(const char *fmt, ...) { fputs("libc++ abort\n", stderr); abort(); }
/* Single-threaded atomics fallbacks (cbe lowers atomics to libcalls). Plan 9 port: processes, not threads. */
#include <stdint.h>
int __atomic_compare_exchange_8(volatile void *p, void *expected, uint64_t desired, int s, int f) {
	volatile uint64_t *q = p; uint64_t *e = expected; (void)s; (void)f;
	if (*q == *e) { *q = desired; return 1; } *e = *q; return 0;
}
/* std::exception key functions (libc++abi normally provides these; stubbed because exceptions are off).
 * With exceptions (-DEH, run_eh.sh) libc++abi's own are translated with the program. */
#ifndef EH
void _ZNSt9exceptionD2Ev(void *t)  { (void)t; }
void _ZNSt9exceptionD1Ev(void *t)  { (void)t; }
void _ZNSt9exceptionD0Ev(void *t)  { free(t); }
const char *_ZNKSt9exception4whatEv(void *t) { (void)t; return "std::exception"; }
void _ZNSt8bad_castD2Ev(void *t)   { (void)t; }
#endif
