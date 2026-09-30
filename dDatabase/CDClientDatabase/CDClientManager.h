#ifndef __CDCLIENTMANAGER__H__
#define __CDCLIENTMANAGER__H__

#define UNUSED_TABLE(v)

#include <filesystem>

/**
 * Initialize the CDClient tables so they are all loaded into memory.
 */
namespace CDClientManager {
	/**
	 * @param fdbPath The client's cdclient.fdb. When it opens, the tables looked up by their first
	 * column read their rows from it (shared between processes) instead of caching them. Empty, or
	 * a file that doesn't open, keeps every table on CDServer.sqlite.
	 */
	void LoadValuesFromDatabase(const std::filesystem::path& fdbPath = {});
	void LoadValuesFromDefaults();

	/**
	 * Loads every table again after a CDClient reload (the caller has already reconnected CDClientDatabase to the new
	 * CDServer.sqlite). Each table's old entries are emptied out but kept alive, and so is the old fdb's view, so a
	 * reference an entity took from a table before the reload stays valid: what is spawned keeps what it loaded, and
	 * what is made afterwards reads the new data. Main thread only.
	 */
	void Reload(const std::filesystem::path& fdbPath);

	// The first half of Reload: lets go of the fdb and empties every table, keeping the old entries alive
	void ResetTables();

	// How many tables ResetTables empties
	uint32_t GetTableCount();

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
