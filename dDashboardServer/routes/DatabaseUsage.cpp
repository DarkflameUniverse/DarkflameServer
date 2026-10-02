#include "DatabaseUsage.h"

#include <ctime>
#include <filesystem>

#include "Alerts.h"
#include "Background.h"
#include "Database.h"
#include "DatabaseUsageReport.h"
#include "dConfig.h"
#include "eHTTPMethod.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "RouteUtils.h"
#include "Scheduler.h"

using namespace RouteUtils;

namespace {
	constexpr const char* VIEW = "health_view";
	constexpr int64_t DAY = 24 * 60 * 60;
	constexpr size_t CHART_POINTS = 240;
	constexpr size_t FASTEST = 10;

	// What the last measurement found that isn't stored (main thread only)
	struct Last {
		std::string backend;
		std::string location;
		std::string diskPath;
		bool sizesKnown{ true };
		bool diskLow{};
	} g_Last;

	double WarnPercent() {
		if (!Game::config) return 10;
		return GeneralUtils::TryParse<double>(Game::config->GetValue("database_disk_warn_percent")).value_or(10);
	}

	std::string FormatBytes(uint64_t bytes) {
		const char* units[] = { "B", "KB", "MB", "GB", "TB" };
		double value = static_cast<double>(bytes);
		int unit = 0;
		while (value >= 1024 && unit < 4) { value /= 1024; unit++; }
		char text[32];
		std::snprintf(text, sizeof(text), unit == 0 ? "%.0f %s" : "%.1f %s", value, units[unit]);
		return text;
	}

	void Measure(Scheduler::RunPtr run) {
		// Config on the main thread; the worker only gets plain values
		const auto backend = Database::GetMigrationFolder();
		const auto mysqlHost = Game::config ? Game::config->GetValue("mysql_host") : "";
		const auto diskOverride = Game::config ? Game::config->GetValue("database_disk_path") : "";
		const bool localDatabase = backend == "sqlite" || DatabaseUsageReport::IsLocalHost(mysqlHost);
		run->Log("Measuring the " + backend + " database" + (diskOverride.empty() ? "" : ", disk space of " + diskOverride + " (database_disk_path)"));

		const bool queued = Background::Run("database_usage", [diskOverride, localDatabase](GameDatabase& db) -> nlohmann::json {
			const auto measurement = db.MeasureDatabaseUsage();
			IDatabaseUsage::UsageTotals totals;
			totals.time = static_cast<int64_t>(std::time(nullptr));
			totals.totalBytes = measurement.totalBytes;
			totals.freeBytes = measurement.freeBytes;
			for (const auto& table : measurement.tables) {
				totals.dataBytes += table.dataBytes;
				totals.indexBytes += table.indexBytes;
				totals.rows += table.rows;
			}
			// The database's volume: what database_disk_path says, else the SQLite file or a local MySQL's data directory
			const std::string diskPath = !diskOverride.empty() ? diskOverride : localDatabase ? measurement.location : "";
			std::string diskError;
			if (!diskPath.empty()) {
				std::error_code ec;
				const auto space = std::filesystem::space(diskPath, ec);
				if (ec) diskError = ec.message();
				else {
					totals.diskFree = space.available;
					totals.diskTotal = space.capacity;
				}
			}
			db.InsertDatabaseUsage(totals, measurement.tables);
			return { {"time", totals.time}, {"total", totals.totalBytes}, {"tables", measurement.tables.size()}, {"location", measurement.location},
				{"sizes_known", measurement.sizesKnown}, {"disk_path", diskPath}, {"disk_error", diskError}, {"disk_free", totals.diskFree},
				{"disk_total", totals.diskTotal} };
		}, [run, backend](nlohmann::json result, const std::string& error) {
			if (!error.empty()) return run->Finish(false, "Failed: " + error);
			g_Last.backend = backend;
			g_Last.location = result.value("location", "");
			g_Last.diskPath = result.value("disk_path", "");
			g_Last.sizesKnown = result.value("sizes_known", true);
			if (!g_Last.sizesKnown) run->Log("SQLite has no dbstat table: only the total size and row counts are known");
			const auto total = result.value("total", uint64_t{});
			run->Log(std::to_string(result.value("tables", 0)) + " tables, " + FormatBytes(total) + " (" + g_Last.location + ")");
			const auto diskError = result.value("disk_error", "");
			if (!diskError.empty()) run->Log("Free disk space of " + g_Last.diskPath + " unknown: " + diskError);
			const auto disk = DatabaseUsageReport::DiskState(result.value("disk_free", uint64_t{}), result.value("disk_total", uint64_t{}), WarnPercent());
			if (disk.known) {
				run->Log("Disk: " + FormatBytes(disk.free) + " free of " + FormatBytes(disk.total));
				if (disk.low && !g_Last.diskLow) {
					Alerts::Emit("server", "Database disk space low", "Only " + FormatBytes(disk.free) + " of " + FormatBytes(disk.total) +
						" is free on the database's disk (warning under database_disk_warn_percent). The database is " + FormatBytes(total) + ".", {}, "/database_usage");
				}
				g_Last.diskLow = disk.low;
			} else if (g_Last.diskPath.empty()) {
				run->Log("Free disk space not checked: the database is on another machine (set database_disk_path to check a local path)");
			}
			run->Finish(true, FormatBytes(total) + (disk.low ? ", disk space low" : ""));
		});
		if (!queued) run->Finish(false, "A measurement is already running");
	}
}

namespace DatabaseUsage {
	void RegisterTask() {
		Scheduler::Register({ "database_usage", "Database usage",
			"Measures the game database (its size and each table's data, indexes and rows) and the free space on its disk for the Database usage page, "
			"and alerts when the disk's free space drops under database_disk_warn_percent. Samples are kept for database_usage_days. "
			"On a big SQLite database it counts every table's rows, on the background connection.",
			"0 * * * *", Measure, 60 * 60 });
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/api/database/usage", Perm(VIEW),
			"Database usage over a range. Query: ?range=day|week|month (default week). The latest sample's totals and disk space, the growth chart's "
			"series, every table now with its growth over the range, and the fastest-growing tables",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto range = QueryValue(context.queryString, "range");
				const int64_t span = range == "day" ? DAY : range == "month" ? 30 * DAY : 7 * DAY;
				const auto now = static_cast<int64_t>(std::time(nullptr));
				const auto samples = Database::Get()->GetDatabaseUsageTotals(now - span, now);
				const auto warn = WarnPercent();
				nlohmann::json out = {
					{"range", range == "day" || range == "month" ? range : "week"}, {"warn_percent", warn}, {"backend", Database::GetMigrationFolder()},
					{"location", g_Last.location}, {"disk_path", g_Last.diskPath}, {"sizes_known", g_Last.sizesKnown},
					{"can_run", Can(context, "tasks_manage")}, {"running", Background::IsRunning("database_usage")},
				};
				if (samples.empty()) {
					out["latest"] = nullptr;
					out["series"] = DatabaseUsageReport::Series({}, CHART_POINTS);
					out["tables"] = nlohmann::json::array();
					out["fastest"] = nlohmann::json::array();
					return JsonSuccess(reply, out);
				}
				const auto& first = samples.front();
				const auto& last = samples.back();
				out["latest"] = DatabaseUsageReport::TotalsJson(last, warn);
				out["first"] = DatabaseUsageReport::TotalsJson(first, warn);
				out["series"] = DatabaseUsageReport::Series(samples, CHART_POINTS);
				const auto tables = DatabaseUsageReport::Tables(Database::Get()->GetDatabaseUsageTables(last.time),
					samples.size() > 1 ? Database::Get()->GetDatabaseUsageTables(first.time) : std::vector<IDatabaseUsage::TableUsage>{}, last.time - first.time);
				out["fastest"] = samples.size() > 1 ? DatabaseUsageReport::Fastest(tables, FASTEST) : nlohmann::json::array();
				out["tables"] = tables;
				JsonSuccess(reply, out);
			});
	}
}
