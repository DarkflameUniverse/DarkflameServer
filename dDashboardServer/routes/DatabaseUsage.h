#pragma once

/**
 * Database usage (docs/Dashboard.md, "Database usage"): a scheduled task (database_usage, hourly by default) measures
 * the game database (whole size, each table's data, indexes and rows) and the free space on its disk on the
 * background connection, keeps the samples for database_usage_days, and alerts when the disk's free space drops under
 * database_disk_warn_percent. The page shows the totals, the growth chart, the fastest-growing tables and every table.
 */
namespace DatabaseUsage {
	// Before Scheduler::Initialize
	void RegisterTask();

	void RegisterRoutes();
}
