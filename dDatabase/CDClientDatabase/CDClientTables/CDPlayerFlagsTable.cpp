#include "CDPlayerFlagsTable.h"

#include "CDClientDatabase.h"

namespace CDPlayerFlagsTable {
	Table entries;

	void ReadEntry(CppSQLite3Query& table) {
		Entry entry;
		entry.sessionOnly = table.getIntField("SessionOnly") == 1;
		entry.onlySetByServer = table.getIntField("OnlySetByServer") == 1;
		entry.sessionZoneOnly = table.getIntField("SessionZoneOnly") == 1;
		entries[table.getIntField("id")] = entry;
	}

	void LoadValuesFromDatabase() {
		auto table = CDClientDatabase::ExecuteQuery("SELECT * FROM PlayerFlags;");

		if (!table.eof()) {
			do {
				ReadEntry(table);
			} while (!table.nextRow());
		}
	}

	const std::optional<Entry> GetEntry(const FlagId flagId) {
		if (!entries.contains(flagId)) {
			auto table = CDClientDatabase::CreatePreppedStmt("SELECT * FROM PlayerFlags WHERE id = ?;");
			table.bind(1, static_cast<int>(flagId));
			auto result = table.execQuery();
			if (!result.eof()) {
				ReadEntry(result);
			}
		}

		return entries[flagId];
	}
}
