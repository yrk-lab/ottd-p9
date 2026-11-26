// p1: pointers to member functions: non-virtual, virtual, through a second base
// (the this-adjustment), and kept in a table as libc++'s call_once and OpenTTD's
// command tables do.  On Plan 9 functions can have odd addresses (6l does not
// align them), which the Itanium ABI's virtual bit must not mistake: eh/pmf.py.
#include <cstdio>

struct Base {
	int x;
	Base(int v) : x(v) {}
	virtual ~Base() {}
	int plain() { return x; }
	virtual int virt() { return x + 100; }
};

struct Other {
	int y = 7;
	virtual ~Other() {}
	int other() { return y; }
	virtual int otherv() { return y * 3; }
};

struct Derived : Base, Other {
	Derived(int v) : Base(v) {}
	int virt() override { return x + 200; }
	int otherv() override { return y * 5; }
	int own() { return x * 2; }
};

template <class C>
static int call(C *c, int (C::*m)())
{
	return (c->*m)();
}

int main()
{
	Derived d(4);
	Base b(9);
	int (Base::*bm[])() = { &Base::plain, &Base::virt };
	int (Derived::*dm[])() = { &Derived::own, &Derived::virt, &Derived::plain, &Derived::other, &Derived::otherv };
	for (auto m : bm) printf("base %d derived-as-base %d\n", call(&b, m), call<Base>(&d, m));
	for (auto m : dm) printf("derived %d\n", call(&d, m));
	int (Other::*om)() = &Other::otherv;
	Other *o = &d;
	printf("other via second base %d %d\n", (o->*om)(), call(o, &Other::other));
	return 0;
}
