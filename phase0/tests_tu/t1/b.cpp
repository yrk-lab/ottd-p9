#include "shared.h"

static struct InitB {
	InitB() { registry().push_back("ctor b"); }
} init_b;

void b_register() { registry().push_back("b"); Counter<int>::bump(); }
Fn b_twice() { return twice; }
int b_count() { return Counter<int>::count + Counter<char>::count; }
