// e2: class hierarchies: catch by base reference and pointer, virtual what(), handler order,
// the exception object is destroyed exactly once, multiple inheritance adjusts the pointer.
#include <cstdio>

static int live = 0; // exception objects alive

struct Error {
	int code;
	Error(int c) : code(c) { live++; }
	Error(const Error &o) : code(o.code) { live++; }
	virtual ~Error() { live--; }
	virtual const char *what() const { return "Error"; }
};
struct IOError : Error {
	IOError(int c) : Error(c) {}
	const char *what() const override { return "IOError"; }
};
struct DiskError : IOError {
	DiskError(int c) : IOError(c) {}
	const char *what() const override { return "DiskError"; }
};
struct Tag {
	int tag = 77;
	virtual ~Tag() {}
};
struct Tagged : Tag, Error { // Error is the second base: catching it needs a pointer adjustment
	Tagged(int c) : Error(c) {}
	const char *what() const override { return "Tagged"; }
};

static void raise(int k)
{
	switch (k) {
	case 0:
		throw Error(1);
	case 1:
		throw IOError(2);
	case 2:
		throw DiskError(3);
	case 3:
		throw Tagged(4);
	case 4:
		throw new DiskError(5); // by pointer
	}
}

static void by_ref(int k)
{
	try {
		raise(k);
	} catch (DiskError &e) {
		printf("  DiskError& %s %d\n", e.what(), e.code);
	} catch (IOError &e) {
		printf("  IOError& %s %d\n", e.what(), e.code);
	} catch (const Error &e) {
		printf("  Error& %s %d\n", e.what(), e.code);
	} catch (Error *p) {
		printf("  Error* %s %d\n", p->what(), p->code);
		delete p;
	}
}

static void by_tag()
{
	try {
		raise(3);
	} catch (Tag &t) {
		printf("  Tag& %d\n", t.tag);
	}
}

int main()
{
	for (int k = 0; k < 5; k++) {
		printf("raise(%d):\n", k);
		by_ref(k);
		printf("  live = %d\n", live);
	}
	by_tag();
	printf("live = %d\n", live);
	return 0;
}
