#pragma once

#include <string>
#include <typeinfo>

std::string demangle(const char* name);

template <class T>
std::string type(const T& t) {

	return demangle(typeid(t).name());
}
