#pragma once

#include "CDClientDatabase.h"
#include "CDClientManager.h"
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

// Use this to skip unused defaults for unused entries in some tables
#define UNUSED_ENTRY(v, x)

#pragma warning (disable : 4244) //Disable double to float conversion warnings
// #pragma warning (disable : 4715) //Disable "not all control paths return a value"

template<class Table, typename Storage>
class CDTable : public Singleton<Table> {
public:
	typedef Storage StorageType;

protected:
	virtual ~CDTable() = default;

	// If you need these for a specific table, override it such that there is a public variant.
	[[nodiscard]] StorageType& GetEntriesMutable() const {
		return CDClientManager::GetEntriesMutable<Table>();
	}

	// If you need these for a specific table, override it such that there is a public variant.
	[[nodiscard]] const StorageType& GetEntries() const {
		return GetEntriesMutable();
	}
};
