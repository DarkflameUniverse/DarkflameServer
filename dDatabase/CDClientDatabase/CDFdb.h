#ifndef CDFDB_H
#define CDFDB_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "FdbReader.h"

/**
 * Reading CDClient tables straight from the client's cdclient.fdb.
 *
 * The fdb is mapped read-only, so every server process on the machine shares one copy of it
 * through the OS page cache, and a row is found through the fdb's own hash buckets instead of
 * a per-process cache of the whole table.
 *
 * CDServer.sqlite stays the source of truth: the server's CDServer migrations change a few rows
 * of it, so at load each table compares its rows in both files and reads the keys that differ
 * from SQLite (see FindChangedKeys).
 */
namespace CDFdb {
	/**
	 * Opens the fdb. Does nothing and returns false if the file is missing or unreadable, and
	 * then the tables keep reading CDServer.sqlite as before.
	 */
	bool Open(const std::filesystem::path& path, bool allowMapping = true);
	void Close();

	/**
	 * Closes the fdb for a CDClient reload, but keeps its view open until the next Retire, so a row or table pointer
	 * handed out before the reload still reads from memory that is there. Only the latest retired fdb is kept.
	 */
	void Retire();

	// The open fdb, or nullptr
	const FdbReader* Get();

	/**
	 * The fdb table that can stand in for the SQLite table of the same name: it exists in both,
	 * has the same columns in the same order, and its first column is an integer. nullptr otherwise.
	 */
	const FdbReader::Table* GetTable(const std::string& name);

	/**
	 * The first-column keys whose rows differ between the fdb table and the SQLite table of the
	 * same name (values compared by type and content, rows as a set per key). Keys only in one of
	 * the two are included. nullopt if the tables can't be compared.
	 */
	std::optional<std::vector<int64_t>> FindChangedKeys(const FdbReader::Table& table);

	/**
	 * Reads one fdb row with the same accessors, defaults and conversions as CppSQLite3Query, so
	 * a table can fill its entries from either source with the same code.
	 */
	class RowFields {
	public:
		RowFields(const FdbReader::Table& table, const FdbReader::Row& row);

		int getIntField(const char* field, int nullValue = 0) const;
		int64_t getInt64Field(const char* field, int64_t nullValue = 0) const;
		double getFloatField(const char* field, double nullValue = 0.0) const;
		std::string getStringField(const char* field, const char* nullValue = "") const;
		bool fieldIsNull(const char* field) const;

	private:
		// -1 when the table has no such column, which reads as null
		int32_t Column(const char* field) const;

		const FdbReader::Table& m_Table;
		const FdbReader::Row& m_Row;
	};
};

#endif // CDFDB_H
