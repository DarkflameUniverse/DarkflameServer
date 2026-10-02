#ifndef __IDATABASEUSAGE__H__
#define __IDATABASEUSAGE__H__

#include <cstdint>
#include <string>
#include <vector>

/**
 * How much room the game database takes, measured by the dashboard's Database usage task (hourly by default) and
 * kept as history for database_usage_days: the whole database and every table's data, indexes and rows.
 */
class IDatabaseUsage {
public:
	struct TableUsage {
		std::string name;
		uint64_t dataBytes{};
		uint64_t indexBytes{};
		uint64_t rows{};       // MySQL/MariaDB: the server's estimate (information_schema); SQLite: counted
	};

	// The database as it is now
	struct Measurement {
		std::vector<TableUsage> tables; // by name
		uint64_t totalBytes{};          // SQLite: the file's pages; MySQL: every table's data and indexes
		uint64_t freeBytes{};           // allocated but unused: SQLite free pages, MySQL data_free (both reusable)
		std::string location;           // SQLite: the database file; MySQL: the server's data directory (@@datadir)
		bool sizesKnown{ true };        // false: SQLite without the dbstat table, so only the totals and rows are known
	};

	// Reads sizes only; slow on a big SQLite database (counts every table's rows), so run it in the background
	virtual Measurement MeasureDatabaseUsage() = 0;

	// One sample of the whole database
	struct UsageTotals {
		int64_t time{};
		uint64_t totalBytes{};
		uint64_t freeBytes{};
		uint64_t dataBytes{};  // the tables'
		uint64_t indexBytes{};
		uint64_t rows{};
		uint64_t diskFree{};   // on the database's volume; diskTotal 0: not known (a database on another machine)
		uint64_t diskTotal{};
	};

	virtual void InsertDatabaseUsage(const UsageTotals& totals, const std::vector<TableUsage>& tables) = 0;

	// The samples between from and to (inclusive), oldest first
	virtual std::vector<UsageTotals> GetDatabaseUsageTotals(int64_t from, int64_t to) = 0;

	// The tables of the sample taken at `time`, by name
	virtual std::vector<TableUsage> GetDatabaseUsageTables(int64_t time) = 0;

	// Removes samples (and their tables) older than beforeTime; returns how many samples
	virtual uint32_t PruneDatabaseUsage(int64_t beforeTime) = 0;
};

#endif  //!__IDATABASEUSAGE__H__
