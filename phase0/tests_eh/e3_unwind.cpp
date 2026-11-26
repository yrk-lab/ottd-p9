// e3: unwinding runs destructors in every frame (RAII, std::string, std::vector),
// rethrow with `throw;`, exceptions through virtual calls and function pointers,
// and many throws without leaking (counted live objects).
#include <cstdio>
#include <string>
#include <vector>

static int live = 0;
static std::string trace;

struct Guard {
	char name;
	Guard(char n) : name(n) { live++; trace += '+'; trace += n; }
	~Guard() { live--; trace += '-'; trace += name; }
};

struct Step {
	virtual ~Step() {}
	virtual int run(int depth) = 0;
};
struct Deep : Step {
	int run(int depth) override
	{
		Guard g('a' + depth);
		std::vector<std::string> v(3, std::string(40, 'x'));	// heap memory owned by the frame
		if (depth == 0) throw depth + 100;
		return this->run(depth - 1) + 1;
	}
};

static int via_pointer(int (*f)(int), int x)
{
	Guard g('P');
	return f(x);
}
static int boom(int x)
{
	Guard g('B');
	if (x > 0) throw std::string("boom ") + std::to_string(x);
	return 0;
}

static int rethrower()
{
	Guard g('R');
	try {
		Deep d;
		return d.run(3);
	} catch (int v) {
		trace += "!caught";
		throw;	// rethrow the same object
	}
}

int main()
{
	try {
		rethrower();
	} catch (int v) {
		printf("outer caught %d\n", v);
	}
	printf("trace %s\nlive %d\n", trace.c_str(), live);

	trace.clear();
	try {
		via_pointer(boom, 3);
	} catch (const std::string &s) {
		printf("caught \"%s\"\n", s.c_str());
	}
	printf("trace %s\nlive %d\n", trace.c_str(), live);

	long n = 0;
	for (int i = 0; i < 2000; i++) {
		trace.clear();
		try {
			Deep d;
			d.run(i % 5);
		} catch (int v) {
			n += v;
		}
	}
	printf("n %ld live %d\n", n, live);
	return 0;
}
