// Tier 2: classes, virtuals, ctors/dtors, static init, templates, lambdas, namespaces.
// Mirrors OpenTTD style (pool items with virtual dtors, template helpers, RAII).
#include <stdio.h>
#include <stdint.h>
namespace ns {
struct Base {
	virtual ~Base() { live--; }
	virtual int f(int v) const = 0;
	static inline int live = 0;
	Base() { live++; }
};
struct A : Base {
	int f(int v) const override { return v + 1; }
};
struct B : Base {
	int k;
	explicit B(int k) : k(k) {}
	int f(int v) const override { return v * k; }
};
template <typename T, int N>
struct Arr {
	T d[N];
	constexpr T &operator[](int i) { return d[i]; }
	constexpr int size() const { return N; }
};
template <typename F>
int apply(F &&fn, int v)
{
	return fn(v);
}
struct Guard {
	int *p;
	Guard(int *p) : p(p) { *p += 100; }
	~Guard() { *p -= 1; }
};
} // namespace ns
static int g_init = [] { return 7; }(); // dynamic static init
static ns::Arr<int, 8> table = { { 1, 2, 3, 4, 5, 6, 7, 8 } };
int main()
{
	int counter = 0;
	{
		ns::Guard gd(&counter);
		ns::Base *objs[2] = { new ns::A, new ns::B(3) };
		int acc = g_init;
		for (auto *o : objs) acc = o->f(acc);
		for (int i = 0; i < table.size(); i++) acc += table[i];
		acc += ns::apply([&](int v) { return v + counter; }, 5);
		printf("t2 %d live=%d\n", acc, ns::Base::live);
		for (auto *o : objs) delete o;
	}
	printf("t2 counter=%d live=%d\n", counter, ns::Base::live);
	return 0;
}
