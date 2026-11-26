// e5: the patterns OpenTTD uses.
//  - saveload: an error deep in a reader throws; the top level catches, and RAII
//    guards restore global state on the way out
//  - script engine: a C-style interpreter loop calls native functions through
//    function pointers; a native function "suspends" by throwing a marker object
//    that the loop catches, and the loop resumes later
//  - constructors that throw: fully built members are destroyed, the rest are not
//  - an exception escaping a handler of another exception
#include <cstdio>
#include <string>
#include <vector>

/* saveload */
static int state = 0; // global the loader changes and must restore
struct SaveState {
	int old;
	SaveState(int v) : old(state) { state = v; }
	~SaveState() { state = old; }
};
struct LoadError {
	std::string msg;
};

static int read_byte(const std::vector<int> &buf, size_t &pos)
{
	if (pos >= buf.size()) throw LoadError { "unexpected end of savegame at " + std::to_string(pos) };
	return buf[pos++];
}
static int read_chunk(const std::vector<int> &buf, size_t &pos)
{
	SaveState s(2);
	int n = read_byte(buf, pos), sum = 0;
	for (int i = 0; i < n; i++) sum += read_byte(buf, pos);
	return sum;
}
static void load(const std::vector<int> &buf)
{
	SaveState s(1);
	size_t pos = 0;
	try {
		int total = 0;
		while (pos < buf.size()) total += read_chunk(buf, pos);
		printf("loaded, total %d, state %d\n", total, state);
	} catch (const LoadError &e) {
		printf("load failed: %s, state %d\n", e.msg.c_str(), state);
	}
}

/* script engine */
struct Suspend {
	int ticks;
};
typedef int (*Native)(int);
static int n_add(int x)
{
	return x + 1;
}
static int n_sleep(int x)
{
	if (x % 3 == 0) throw Suspend { x };
	return x;
}
static int n_fail(int x)
{
	if (x == 7) throw std::string("script error");
	return x;
}

static int vm_run(const Native *prog, int len, int &pc, int acc)
{
	for (; pc < len; pc++) acc = prog[pc](acc);
	return acc;
}

/* constructors */
static std::string ctor_trace;
struct Part {
	char c;
	Part(char c, bool fail) : c(c)
	{
		if (fail) throw c;
		ctor_trace += '+';
		ctor_trace += c;
	}
	~Part()
	{
		ctor_trace += '-';
		ctor_trace += c;
	}
};
struct Whole {
	Part a, b, c;
	Whole(int failat) : a('a', failat == 0), b('b', failat == 1), c('c', failat == 2) {}
};

int main()
{
	load({ 2, 10, 20, 1, 5 });
	load({ 2, 10, 20, 3, 5 });
	printf("state after %d\n", state);

	Native prog[] = { n_add, n_sleep, n_add, n_fail, n_add, n_sleep, n_add };
	int pc = 0, acc = 0, resumes = 0;
	for (;;) {
		try {
			acc = vm_run(prog, 7, pc, acc);
			break;
		} catch (const Suspend &s) {
			resumes++;
			pc++; // resume after the suspending native
		} catch (const std::string &e) {
			printf("vm: %s at pc %d\n", e.c_str(), pc);
			break;
		}
	}
	printf("vm acc %d pc %d resumes %d\n", acc, pc, resumes);

	for (int f = 0; f <= 3; f++) {
		ctor_trace.clear();
		try {
			Whole w(f);
			ctor_trace += " built";
		} catch (char c) {
			ctor_trace += " failed ";
			ctor_trace += c;
		}
		printf("Whole(%d): %s\n", f, ctor_trace.c_str());
	}

	try {
		try {
			throw 1;
		} catch (int) {
			throw std::string("from handler");
		}
	} catch (const std::string &s) {
		printf("caught %s\n", s.c_str());
	}
	return 0;
}
