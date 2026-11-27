#include "shared.h"

static struct InitA {
	InitA() { registry().push_back("ctor a"); }
} init_a;

int next_id() { static int n = 100; return n++; }
void a_register() { registry().push_back("a"); Counter<int>::bump(); }
Fn a_twice() { return twice; }
Shape *a_make() { return new Square; }
void a_throw(int c) { throw Oops{ c }; }
