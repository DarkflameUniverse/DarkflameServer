#pragma once

#include "CDClientDatabase.h"
#include "Singleton.h"

#include <functional>
#include <string>
#include <vector>
#include <map>

// CPPLinq
#ifdef _WIN32
#define NOMINMAX
// windows.h has min and max macros that breaks cpplinq
#endif
#include "cpplinq.hpp"

#pragma warning (disable : 4244) //Disable double to float conversion warnings
#pragma warning (disable : 4715) //Disable "not all control paths return a value"

template<class Table>
class CDTable : public Singleton<Table> {
protected:
	virtual ~CDTable() = default;
};
