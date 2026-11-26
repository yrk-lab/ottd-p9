// e1: throw and catch builtin types; catch(...); nested try; not-thrown paths.
#include <cstdio>

static int thrower(int x)
{
	if (x == 1) throw 42;
	if (x == 2) throw 2.5;
	if (x == 3) throw "text";
	if (x == 4) throw 'c';
	return x * 10;
}

static int classify(int x)
{
	try {
		return thrower(x);
	} catch (int v) {
		return 1000 + v;
	} catch (double d) {
		return 2000 + (int)(d * 10);
	} catch (const char *s) {
		return 3000 + s[0];
	} catch (...) {
		return 4000;
	}
}

static int nested(int x)
{
	int r = 0;
	try {
		try {
			thrower(x);
			r += 1;
		} catch (double) {
			r += 10;
			throw 7;	// new exception from a handler
		}
		r += 100;
	} catch (int v) {
		r += 1000 * v;
	}
	return r;
}

int main()
{
	for (int i = 0; i <= 5; i++) printf("classify(%d) = %d\n", i, classify(i));
	for (int i = 0; i <= 2; i++) printf("nested(%d) = %d\n", i, nested(i));
	int sum = 0;
	for (int i = 0; i < 10000; i++) sum += classify(i % 3);	// many throw/catch cycles
	printf("sum = %d\n", sum);
	return 0;
}
