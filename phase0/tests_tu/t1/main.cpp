#include "shared.h"

int main()
{
	a_register();
	b_register();
	registry().push_back("main");
	size_t ctors = 0;
	for (auto &s : registry())
		if (s.compare(0, 4, "ctor") == 0)
			ctors++;
	std::printf("registry %zu entries, %zu from constructors, first %s\n", registry().size(), ctors, registry()[0].c_str());
	std::printf("same twice: %d, twice(21) = %d\n", a_twice() == b_twice(), a_twice()(21));
	std::printf("counts %d\n", b_count());
	Shape *s = a_make();
	std::printf("made a %s, typeid matches: %d\n", s->name().c_str(), dynamic_cast<Square *>(s) != nullptr);
	delete s;
	try {
		a_throw(42);
	} catch (const Oops &o) {
		std::printf("caught Oops %d\n", o.code);
	}
	return 0;
}
