#ifndef IMIGRATIONHISTORY_H
#define IMIGRATIONHISTORY_H

#include <string_view>

class IMigrationHistory {
public:
	// Create the migration history table.
	virtual void CreateMigrationHistoryTable() = 0;

	// Check if the given migration has been run.
	virtual bool IsMigrationRun(const std::string_view str) = 0;

	// Insert the given migration into the migration history table.
	virtual void InsertMigration(const std::string_view str) = 0;
};
#endif  //!IMIGRATIONHISTORY_H
