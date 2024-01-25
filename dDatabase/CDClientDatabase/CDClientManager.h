#pragma once

#include "CDTable.h"

#include "Singleton.h"

#define UNUSED_TABLE(v)

/**
 * Initialize the CDClient tables so they are all loaded into memory.
 */
class CDClientManager : public Singleton<CDClientManager> {
public:
	CDClientManager() = default;

	void LoadValuesFromDatabase();
	void LoadValuesFromDefaults();

	/**
	 * Fetch a table from CDClient
	 * 
	 * @tparam Table type to fetch
	 * @return A pointer to the requested table.
	 */
	template<typename T>
	T* GetTable() {
		return &T::Instance();
	}
};
