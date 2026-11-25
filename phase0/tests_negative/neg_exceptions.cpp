// MUST FAIL at stage 1-clang. Proves -fno-exceptions turns try/catch into a hard error instead of
// letting cbe's LowerInvokePass silently delete the handler. OpenTTD's few try/catch sites must be
// rewritten (setjmp/longjmp or status returns) in Phase 1.
#include <cstdio>
int f(int x) { try { if (x) throw 1; } catch (int) { return 7; } return 0; }
int main() { printf("%d\n", f(1)); }
