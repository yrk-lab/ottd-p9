// Catch-clause matching for eh_rt.c, using libc++abi's own typeinfo classes
// (private_typeinfo.cpp, translated with the program): the thrown type matches
// if it is the catch type, a public unambiguous derived class, or a pointer
// conversion of it; *adjusted becomes the pointer the handler receives.
#include "private_typeinfo.h"

extern "C" int
eh_can_catch(void *catchtype, void *throwntype, void **adjusted)
{
	auto c = static_cast<const __cxxabiv1::__shim_type_info *>(catchtype);
	auto t = static_cast<const __cxxabiv1::__shim_type_info *>(throwntype);
	return c->can_catch(t, *adjusted);
}
