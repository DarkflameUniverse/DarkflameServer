#include "Demangler.h"
#ifdef __GNUG__
#include <cstdlib>
#include <cxxabi.h>
#include <memory>
#include <typeinfo>

std::string Demangler::Demangle(const char* name) {
	// some arbitrary value to eliminate the compiler warning
	// -4 is not a valid return value for __cxa_demangle so we'll use that.
	int status = -4;

	// __cxa_demangle requires that we free the returned char*
	std::unique_ptr<char, void (*)(void*)> res{
		abi::__cxa_demangle(name, NULL, NULL, &status),
		std::free
	};

	return (status == 0) ? res.get() : "";
}

#else // __GNUG__

// does nothing if not g++
std::string Demangler::Demangle(const char* name) {
	return name;
}

#endif // __GNUG__
