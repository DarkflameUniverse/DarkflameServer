#pragma once

#include <string>

namespace Demangler {
	// Given a char* containing a mangled name, return a std::string containing the demangled name.
	// If the function fails for any reason, it returns an empty string.
	std::string Demangle(const char* name);
}
