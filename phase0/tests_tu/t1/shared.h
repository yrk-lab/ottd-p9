/* t1: what C++ puts in every unit that uses it, shared by three units. */
#include <cstdio>
#include <string>
#include <vector>

int next_id();

/* an inline function with a static local, initialized at run time */
inline std::vector<std::string> &registry()
{
	static std::vector<std::string> r{ "first" };
	return r;
}

/* a template with a static data member */
template <class T> struct Counter {
	static int count;
	static void bump() { count += 10; }
};
template <class T> int Counter<T>::count = next_id();

/* inline virtual functions: the vtable and typeinfo are in every unit */
struct Shape {
	virtual ~Shape() {}
	virtual std::string name() const { return "shape"; }
};
struct Square : Shape {
	std::string name() const override { return "square"; }
};

/* an exception type thrown in one unit, caught in another */
struct Oops {
	int code;
};

inline int twice(int x) { return 2 * x; }
typedef int (*Fn)(int);

void a_register();
Fn a_twice();
Shape *a_make();
void a_throw(int);
void b_register();
Fn b_twice();
int b_count();
