#ifndef __CDCLIENTMANAGER__H__
#define __CDCLIENTMANAGER__H__

#define UNUSED_TABLE(v)

/**
 * Initialize the CDClient tables so they are all loaded into memory.
 */
namespace CDClientManager {
	void LoadValuesFromDatabase();
	void LoadValuesFromDefaults();

	/**
	 * Fetch a table from CDClient
	 * 
	 * @tparam Table type to fetch
	 * @return A pointer to the requested table.
	 */
	template<typename T>
	T* GetTable();

	/**
	 * Fetch a table from CDClient
	 * Note: Calling this function without a template specialization in CDClientManager.cpp will cause a linker error.
	 * 
	 * @tparam Table type to fetch
	 * @return A pointer to the requested table.
	 */
	template<typename T>
	typename T::StorageType& GetEntriesMutable();
};


// These are included after the CDClientManager namespace declaration as CDTable as of Jan 29 2024 relies on CDClientManager in Templated code.
#include "CDTable.h"

#include "Singleton.h"

template<typename T>
T* CDClientManager::GetTable() {
	return &T::Instance();
};

#endif  //!__CDCLIENTMANAGER__H__
