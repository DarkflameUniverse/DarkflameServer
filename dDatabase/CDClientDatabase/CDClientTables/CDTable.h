#pragma once

#include "CDClientDatabase.h"
#include "Singleton.h"
#include "DluAssert.h"

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

// CPPLinq
#ifdef _WIN32
#define NOMINMAX
// windows.h has min and max macros that breaks cpplinq
#endif
#include "cpplinq.hpp"

// Used for legacy
#define UNUSED(x)

// Enable this to skip some unused columns in some tables
#define UNUSED_COLUMN(v)

// Use this to skip unused defaults for unused columns in some tables
#define UNUSED_DEFAULT(v, x)

#pragma warning (disable : 4244) //Disable double to float conversion warnings
#pragma warning (disable : 4715) //Disable "not all control paths return a value"

template<class Table>
class CDTable : public Singleton<Table> {
protected:
	virtual ~CDTable() = default;
};

template<class T>
class LookupResult {
	typedef std::pair<T, bool> DataType;
public:
	LookupResult() { m_data.first = T(); m_data.second = false; };
	LookupResult(T& data) { m_data.first = data; m_data.second = true; };
	inline const T& Data() { return m_data.first; };
	inline const bool& FoundData() { return m_data.second; };
private:
	DataType m_data;
};
