#include "MySQLDatabase.h"

IDatabaseUsage::Measurement MySQLDatabase::MeasureDatabaseUsage() {
	Measurement measurement;
	// MySQL 8 caches information_schema sizes for a day by default; this connection asks for fresh ones (MariaDB has no cache)
	{
		auto expiry = ExecuteSelect("SHOW VARIABLES LIKE 'information_schema_stats_expiry';");
		if (expiry->next()) ExecuteCustomQuery("SET SESSION information_schema_stats_expiry = 0;");
	}
	auto result = ExecuteSelect("SELECT TABLE_NAME AS name, COALESCE(DATA_LENGTH, 0) AS data_bytes, COALESCE(INDEX_LENGTH, 0) AS index_bytes, "
		"COALESCE(TABLE_ROWS, 0) AS row_count, COALESCE(DATA_FREE, 0) AS free_bytes FROM information_schema.TABLES "
		"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_TYPE = 'BASE TABLE' ORDER BY TABLE_NAME;");
	while (result->next()) {
		TableUsage table{ result->getString("name").c_str(), result->getUInt64("data_bytes"), result->getUInt64("index_bytes"), result->getUInt64("row_count") };
		measurement.totalBytes += table.dataBytes + table.indexBytes;
		measurement.freeBytes += result->getUInt64("free_bytes");
		measurement.tables.push_back(std::move(table));
	}
	auto dir = ExecuteSelect("SELECT @@datadir AS dir;");
	if (dir->next() && !dir->isNull("dir")) measurement.location = dir->getString("dir").c_str();
	return measurement;
}

void MySQLDatabase::InsertDatabaseUsage(const UsageTotals& t, const std::vector<TableUsage>& tables) {
	const auto prevCommit = GetAutoCommit();
	SetAutoCommit(false);
	ExecuteInsert("REPLACE INTO database_usage (time, total_bytes, free_bytes, data_bytes, index_bytes, row_count, disk_free, disk_total) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?);", t.time, t.totalBytes, t.freeBytes, t.dataBytes, t.indexBytes, t.rows, t.diskFree, t.diskTotal);
	for (const auto& table : tables) {
		ExecuteInsert("REPLACE INTO database_usage_tables (time, table_name, data_bytes, index_bytes, row_count) VALUES (?, ?, ?, ?, ?);",
			t.time, table.name, table.dataBytes, table.indexBytes, table.rows);
	}
	Commit();
	SetAutoCommit(prevCommit);
}

std::vector<IDatabaseUsage::UsageTotals> MySQLDatabase::GetDatabaseUsageTotals(int64_t from, int64_t to) {
	std::vector<UsageTotals> samples;
	auto r = ExecuteSelect("SELECT time, total_bytes, free_bytes, data_bytes, index_bytes, row_count, disk_free, disk_total FROM database_usage "
		"WHERE time >= ? AND time <= ? ORDER BY time;", from, to);
	while (r->next()) {
		samples.push_back({ r->getInt64("time"), r->getUInt64("total_bytes"), r->getUInt64("free_bytes"), r->getUInt64("data_bytes"),
			r->getUInt64("index_bytes"), r->getUInt64("row_count"), r->getUInt64("disk_free"), r->getUInt64("disk_total") });
	}
	return samples;
}

std::vector<IDatabaseUsage::TableUsage> MySQLDatabase::GetDatabaseUsageTables(int64_t time) {
	std::vector<TableUsage> tables;
	auto r = ExecuteSelect("SELECT table_name, data_bytes, index_bytes, row_count FROM database_usage_tables WHERE time = ? ORDER BY table_name;", time);
	while (r->next()) {
		tables.push_back({ r->getString("table_name").c_str(), r->getUInt64("data_bytes"), r->getUInt64("index_bytes"), r->getUInt64("row_count") });
	}
	return tables;
}

uint32_t MySQLDatabase::PruneDatabaseUsage(int64_t beforeTime) {
	ExecuteUpdate("DELETE FROM database_usage_tables WHERE time < ?;", beforeTime);
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM database_usage WHERE time < ?;", beforeTime));
}
