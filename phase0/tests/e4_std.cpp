// e4: exceptions thrown by libc++ itself: std::out_of_range from vector::at and
// string::substr, std::invalid_argument and std::out_of_range from stoi, caught as
// the exact type, as std::logic_error and as std::exception, with what() strings.
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

static void show(const char *what, const std::exception &e)
{
	printf("%s: %s\n", what, e.what());
}

int main()
{
	std::vector<int> v(3, 1);
	try {
		v.at(5) = 1;
	} catch (const std::out_of_range &e) {
		show("vector::at", e);
	}

	std::string s = "openttd";
	try {
		s.substr(99);
	} catch (const std::logic_error &e) {
		show("substr", e);
	}

	const char *inputs[] = { "42", "x9", "99999999999999999999" };
	for (const char *in : inputs) {
		try {
			printf("stoi(%s) = %d\n", in, std::stoi(in));
		} catch (const std::invalid_argument &e) {
			show("invalid_argument", e);
		} catch (const std::exception &e) {
			show("exception", e);
		}
	}

	try {
		throw std::runtime_error(std::string("custom ") + s);
	} catch (const std::exception &e) {
		show("runtime_error", e);
	}
	return 0;
}
