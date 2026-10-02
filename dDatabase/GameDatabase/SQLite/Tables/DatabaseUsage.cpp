#include "SQLiteDatabase.h"

#include <algorithm>
#include <map>

namespace {
	uint64_t U64(CppSQLite3Query& result, const char* field) {
		const auto value = result.getInt64Field(field);
		return value > 0 ? static_cast<uint64_t>(value) : 0;
	}

	uint64_t Pragma(CppSQLite3Query& result) {
		return result.eof() ? 0 : static_cast<uint64_t>(std::max<sqlite_int64>(result.getInt64Field(0), 0));
	}

	std::string Quoted(const std::string& name) {
		std::string quoted = "\"";
		for (const char c : name) quoted += c == '"' ? std::string("\"\"") : std::string(1, c);
		return quoted + "\"";
	}
}

IDatabaseUsage::Measurement SQLiteDatabase::MeasureDatabaseUsage() {
	Measurement measurement;
	uint64_t pageSize = 0, pages = 0, freePages = 0;
	{ auto [s, r] = ExecuteSelect("PRAGMA page_size;"); pageSize = Pragma(r); }
	{ auto [s, r] = ExecuteSelect("PRAGMA page_count;"); pages = Pragma(r); }
	{ auto [s, r] = ExecuteSelect("PRAGMA freelist_count;"); freePages = Pragma(r); }
	measurement.totalBytes = pageSize * pages;
	measurement.freeBytes = pageSize * freePages;
	{
		auto [s, r] = ExecuteSelect("PRAGMA database_list;");
		for (; !r.eof(); r.nextRow()) {
			if (std::string(r.getStringField("name")) == "main") measurement.location = r.getStringField("file");
		}
	}

	std::map<std::string, TableUsage> tables;
	{
		auto [s, r] = ExecuteSelect("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name;");
		for (; !r.eof(); r.nextRow()) {
			const std::string name = r.getStringField("name");
			tables[name].name = name;
		}
	}
	// Bytes of each table and its indexes, from the dbstat table (SQLITE_ENABLE_DBSTAT_VTAB)
	try {
		auto [s, r] = ExecuteSelect("SELECT m.tbl_name AS tbl, m.type AS type, SUM(d.pgsize) AS bytes FROM dbstat d JOIN sqlite_master m ON m.name = d.name "
			"GROUP BY m.tbl_name, m.type;");
		for (; !r.eof(); r.nextRow()) {
			const auto it = tables.find(r.getStringField("tbl"));
			if (it == tables.end()) continue;
			if (std::string(r.getStringField("type")) == "index") it->second.indexBytes += U64(r, "bytes");
			else it->second.dataBytes += U64(r, "bytes");
		}
	} catch (const std::exception&) {
		measurement.sizesKnown = false;
	}
	for (auto& [name, table] : tables) {
		auto [s, r] = ExecuteSelect("SELECT COUNT(*) AS n FROM " + Quoted(name) + ";");
		if (!r.eof()) table.rows = U64(r, "n");
		measurement.tables.push_back(table);
	}
	return measurement;
}

void SQLiteDatabase::InsertDatabaseUsage(const UsageTotals& t, const std::vector<TableUsage>& tables) {
	const auto prevCommit = GetAutoCommit();
	SetAutoCommit(false);
	ExecuteInsert("INSERT OR REPLACE INTO database_usage (time, total_bytes, free_bytes, data_bytes, index_bytes, row_count, disk_free, disk_total) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?);", t.time, t.totalBytes, t.freeBytes, t.dataBytes, t.indexBytes, t.rows, t.diskFree, t.diskTotal);
	for (const auto& table : tables) {
		ExecuteInsert("INSERT OR REPLACE INTO database_usage_tables (time, table_name, data_bytes, index_bytes, row_count) VALUES (?, ?, ?, ?, ?);",
			t.time, table.name, table.dataBytes, table.indexBytes, table.rows);
	}
	Commit();
	SetAutoCommit(prevCommit);
}

std::vector<IDatabaseUsage::UsageTotals> SQLiteDatabase::GetDatabaseUsageTotals(int64_t from, int64_t to) {
	std::vector<UsageTotals> samples;
	auto [s, r] = ExecuteSelect("SELECT time, total_bytes, free_bytes, data_bytes, index_bytes, row_count, disk_free, disk_total FROM database_usage "
		"WHERE time >= ? AND time <= ? ORDER BY time;", from, to);
	for (; !r.eof(); r.nextRow()) {
		samples.push_back({ r.getInt64Field("time"), U64(r, "total_bytes"), U64(r, "free_bytes"), U64(r, "data_bytes"), U64(r, "index_bytes"),
			U64(r, "row_count"), U64(r, "disk_free"), U64(r, "disk_total") });
	}
	return samples;
}

std::vector<IDatabaseUsage::TableUsage> SQLiteDatabase::GetDatabaseUsageTables(int64_t time) {
	std::vector<TableUsage> tables;
	auto [s, r] = ExecuteSelect("SELECT table_name, data_bytes, index_bytes, row_count FROM database_usage_tables WHERE time = ? ORDER BY table_name;", time);
	for (; !r.eof(); r.nextRow()) {
		tables.push_back({ r.getStringField("table_name"), U64(r, "data_bytes"), U64(r, "index_bytes"), U64(r, "row_count") });
	}
	return tables;
}

uint32_t SQLiteDatabase::PruneDatabaseUsage(int64_t beforeTime) {
	ExecuteUpdate("DELETE FROM database_usage_tables WHERE time < ?;", beforeTime);
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM database_usage WHERE time < ?;", beforeTime));
}
