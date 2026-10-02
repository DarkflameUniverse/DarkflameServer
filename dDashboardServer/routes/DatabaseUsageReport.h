#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "IDatabaseUsage.h"
#include "json.hpp"

/**
 * The Database usage page's numbers, worked out from the samples (IDatabaseUsage): the growth chart's series, each
 * table's size and growth over the range, the fastest-growing tables and the disk space warning. Pure (no database,
 * config or clock), so it is unit tested; DatabaseUsage.cpp feeds it.
 */
namespace DatabaseUsageReport {
	// Whether mysql_host is this machine, so the database's data directory can be looked at (localhost, 127.0.0.1, ::1,
	// unix:// or pipe://, with or without tcp:// and a port)
	bool IsLocalHost(const std::string& mysqlHost);

	struct Disk {
		bool known{};
		uint64_t free{};
		uint64_t total{};
		double freePercent{};
		bool low{}; // free below warnPercent of the volume (warnPercent 0: never)
	};
	Disk DiskState(uint64_t free, uint64_t total, double warnPercent);

	// One sample: {time, total_bytes, free_bytes, data_bytes, index_bytes, rows, disk: {known, free, total, free_percent, low}}
	nlohmann::json TotalsJson(const IDatabaseUsage::UsageTotals& totals, double warnPercent);

	// The samples for the chart, at most maxPoints (the last sample of each equal slice of the range):
	// {times, total, data, index, free, rows, disk_free}
	nlohmann::json Series(const std::vector<IDatabaseUsage::UsageTotals>& samples, size_t maxPoints);

	// Every table now with how much it grew since `before` (a table missing then grew by all it has):
	// [{name, data_bytes, index_bytes, total_bytes, rows, growth_bytes, growth_rows, growth_per_day}], biggest first
	nlohmann::json Tables(const std::vector<IDatabaseUsage::TableUsage>& now, const std::vector<IDatabaseUsage::TableUsage>& before, int64_t seconds);

	// From Tables(): the `limit` tables that grew the most bytes (only those that grew)
	nlohmann::json Fastest(const nlohmann::json& tables, size_t limit);
}
