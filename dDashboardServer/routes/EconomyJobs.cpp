#include "EconomyJobs.h"

#include <chrono>
#include <ctime>
#include <map>
#include <set>

#include "RouteUtils.h"
#include "EconomyScan.h"
#include "ReportRoutes.h"
#include "ClientAssets.h"
#include "Alerts.h"
#include "WSRoutes.h"
#include "Background.h"
#include "Scheduler.h"
#include "PlayerActions.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	constexpr int64_t DAY_SECONDS = 24 * 60 * 60;
	constexpr uint32_t HISTORY_DAYS = 30;

	int64_t Setting(const std::string& key, int64_t fallback) {
		if (!Game::config) return fallback;
		return GeneralUtils::TryParse<int64_t>(Game::config->GetValue(key)).value_or(fallback);
	}

	uint32_t Today() {
		return static_cast<uint32_t>(std::time(nullptr) / DAY_SECONDS);
	}

	std::string FormatNumber(int64_t value) {
		auto text = std::to_string(value < 0 ? -value : value);
		for (int i = static_cast<int>(text.size()) - 3; i > 0; i -= 3) text.insert(static_cast<size_t>(i), ",");
		return (value < 0 ? "-" : "") + text;
	}

	// Main thread: turn the day's aggregates (gathered on the worker) into flags
	uint32_t FlagIncome(uint32_t day, const nlohmann::json& raw, int64_t multiplier, int64_t minimum) {
		std::vector<std::pair<LWOOBJID, int64_t>> incomes;
		for (const auto& row : raw) incomes.emplace_back(row[0].get<LWOOBJID>(), row[1].get<int64_t>());
		int64_t median = 0;
		uint32_t added = 0;
		for (const auto& [characterId, earned] : EconomyScan::UnusualIncome(incomes, multiplier, minimum, median)) {
			const auto multiple = median > 0 ? earned / median : 0;
			added += Database::Get()->InsertEconomyFlag({ day, IDashboardAdmin::eFlagKind::COIN_INCOME, characterId, 0, 0, earned, median,
				"Earned " + FormatNumber(earned) + " coins" + (multiple > 0 ? ", " + std::to_string(multiple) + "x the median player (" + FormatNumber(median) + ")" : "") }) ? 1 : 0;
		}
		return added;
	}

	std::map<LOT, int64_t> LotTotals(const nlohmann::json& raw) {
		std::map<LOT, int64_t> totals;
		for (const auto& row : raw) totals[row[0].get<LOT>()] = row[1].get<int64_t>();
		return totals;
	}

	uint32_t FlagItems(uint32_t day, const nlohmann::json& dayRaw, const nlohmann::json& historyRaw, int64_t multiplier, int64_t minimum) {
		uint32_t added = 0;
		for (const auto& spike : EconomyScan::ItemSpikes(LotTotals(dayRaw), LotTotals(historyRaw), HISTORY_DAYS, multiplier, minimum)) {
			added += Database::Get()->InsertEconomyFlag({ day, IDashboardAdmin::eFlagKind::ITEM_SPIKE, 0, spike.lot, 0, spike.created, spike.dailyAverage,
				FormatNumber(spike.created) + "x " + ClientAssets::ItemName(spike.lot) + " created, usually " + FormatNumber(spike.dailyAverage) + " a day" }) ? 1 : 0;
		}
		return added;
	}

	// "Alice (Backpack), Bob (mail)"
	std::string CopyPlaces(const nlohmann::json& copies, bool withItems) {
		std::string where;
		for (const auto& copy : copies) {
			if (!where.empty()) where += ", ";
			if (withItems) where += copy.value("name", "LOT " + std::to_string(copy.value("lot", 0))) + " on ";
			where += copy.value("character_name", copy.value("character_id", "")) + " (" + (copy.value("where", "") == "mail" ? "mail" : copy.value("inventory", "")) + ")";
		}
		return where;
	}

	// Whose login clears a shared id (a scan group's login_fix): "Alice", "Alice or Bob", "Alice and Bob",
	// "all but one of Alice, Bob, Carol", "all of Alice, Bob, Carol"
	std::string WhoMustLogIn(const nlohmann::json& fix) {
		std::vector<std::string> names;
		for (const auto& character : fix.value("characters", nlohmann::json::array())) {
			const auto name = character.value("character_name", "");
			names.push_back(name.empty() ? character.value("character_id", "") : name);
		}
		if (names.empty()) return "";
		if (names.size() == 1) return names[0];
		const bool anyButOne = fix.value("logins_needed", names.size()) < names.size();
		if (names.size() == 2) return names[0] + (anyButOne ? " or " : " and ") + names[1];
		std::string list;
		for (const auto& name : names) list += (list.empty() ? "" : ", ") + name;
		return (anyButOne ? "all but one of " : "all of ") + list;
	}

	uint32_t FlagDuplicates(const nlohmann::json& scan) {
		uint32_t added = 0;
		for (const auto& duplicate : scan["duplicates"]) {
			const auto itemId = GeneralUtils::TryParse<LWOOBJID>(duplicate.value("item_id", "")).value_or(0);
			const auto& copies = duplicate["copies"];
			if (itemId == 0 || copies.empty()) continue;
			auto details = duplicate.value("name", "") + " exists " + std::to_string(copies.size()) + " times: " + CopyPlaces(copies, false);
			// The login migration gives old saves new item ids. That ends the shared id but keeps every copy, so it hides a
			// real dupe rather than fixing it: still flagged, with a note to look before the evidence is gone.
			const auto fix = duplicate.value("login_fix", nlohmann::json::object());
			if (fix.value("resolves", false)) {
				details += ". The IDs split at the next login of " + WhoMustLogIn(fix) + " (an old save: its items get new IDs), so it stops showing up "
					"then, but the copies stay: check it before that";
			}
			const auto owner = GeneralUtils::TryParse<LWOOBJID>(copies[0].value("character_id", "")).value_or(0);
			// Day 0: a duplicated item is flagged once, not again every night
			added += Database::Get()->InsertEconomyFlag({ 0, IDashboardAdmin::eFlagKind::DUPLICATE, owner, duplicate.value("lot", 0), itemId,
				static_cast<int64_t>(copies.size()), 1, details }) ? 1 : 0;
		}
		return added;
	}

	/**
	 * Different items sharing an id come from old saves, whose items had random ids. The login migration gives those
	 * saves new ids, so a collision normally clears itself. One that stays after the characters migrated (or with a
	 * copy in mail, which the migration does not touch) will not: flag it once.
	 */
	uint32_t FlagCollisions(const nlohmann::json& scan) {
		uint32_t added = 0;
		for (const auto& collision : scan.value("collisions", nlohmann::json::array())) {
			if (collision.value("duplicated", false) || collision.value("login_fix", nlohmann::json::object()).value("resolves", false)) continue;
			const auto itemId = GeneralUtils::TryParse<LWOOBJID>(collision.value("item_id", "")).value_or(0);
			const auto& copies = collision["copies"];
			if (itemId == 0 || copies.empty()) continue;
			const auto owner = GeneralUtils::TryParse<LWOOBJID>(copies[0].value("character_id", "")).value_or(0);
			added += Database::Get()->InsertEconomyFlag({ 0, IDashboardAdmin::eFlagKind::ID_COLLISION, owner, copies[0].value("lot", 0), itemId,
				static_cast<int64_t>(copies.size()), 1, std::to_string(copies.size()) + " different items share this object ID and logging in will not "
				"give them new IDs (their characters already migrated, or a copy is in mail): " + CopyPlaces(copies, true) }) ? 1 : 0;
		}
		return added;
	}

	/**
	 * Earlier scans flagged every object id found twice, including ids that old data gave to two different items.
	 * Those aren't dupes: dismiss the open duplicate flags whose id the scan now finds only as an id collision.
	 */
	uint32_t DismissCollisionFlags(const nlohmann::json& scan) {
		std::map<std::string, std::string> collisions; // id -> review note
		for (const auto& collision : scan.value("collisions", nlohmann::json::array())) {
			if (collision.value("duplicated", false)) continue;
			const auto fix = collision.value("login_fix", nlohmann::json::object());
			collisions[collision.value("item_id", "")] = fix.value("resolves", false)
				? "Not a duplicate: different items share this object ID (old random IDs). Resolves on login: the next login of " + WhoMustLogIn(fix) +
					" gives the items of that old save new IDs"
				: "Not a duplicate: different items share this object ID. Logging in will not fix it, so it is flagged as an ID collision instead";
		}
		if (collisions.empty()) return 0;
		uint32_t dismissed = 0;
		const auto flags = Database::Get()->GetEconomyFlagsTable(0, 50000, static_cast<int32_t>(IDashboardAdmin::eFlagStatus::OPEN))["data"];
		for (const auto& flag : flags) {
			const auto note = collisions.find(flag.value("item_id", ""));
			if (flag.value("kind", 0) != static_cast<int>(IDashboardAdmin::eFlagKind::DUPLICATE) || note == collisions.end()) continue;
			const auto id = flag["id"].get<uint64_t>();
			Database::Get()->ReviewEconomyFlag(id, IDashboardAdmin::eFlagStatus::DISMISSED, 0, note->second);
			Audit(SystemContext(), "review_economy_flag", "Flag " + std::to_string(id) + " marked dismissed: object ID " + flag.value("item_id", "") +
				" is shared by different items, not duplicated");
			dismissed++;
		}
		if (dismissed) BroadcastTableChanged("economy_flags");
		return dismissed;
	}

	void Summarize(uint32_t day, uint32_t income, uint32_t items, uint32_t duplicates, uint32_t collisions = 0) {
		const auto total = income + items + duplicates + collisions;
		LOG("Economy checks for day %u: %u new flag(s) (%u income, %u items, %u duplicates, %u id collisions)", day, total, income, items, duplicates, collisions);
		if (total == 0) return;
		std::vector<WebhookFormat::Field> fields;
		if (income) fields.push_back({ "Unusual coin income", std::to_string(income) });
		if (items) fields.push_back({ "Item spikes", std::to_string(items) });
		if (duplicates) fields.push_back({ "Duplicated items", std::to_string(duplicates) });
		if (collisions) fields.push_back({ "Object ID collisions logging in will not fix", std::to_string(collisions) });
		Alerts::Emit("economy_flag", std::to_string(total) + " new economy flag" + (total == 1 ? "" : "s"),
			"The nightly checks found things worth a look.", fields, "/reports#flags");
		BroadcastTableChanged("economy_flags");
	}

	void CompactLedger(Scheduler::RunPtr run) {
		const auto today = Today();
		const auto detailDays = static_cast<uint32_t>(std::max<int64_t>(Setting("economy_detail_days", 180), 31));
		const auto mapDays = static_cast<uint32_t>(std::max<int64_t>(Setting("economy_map_days", 90), 31));
		const auto transferDays = std::max<int64_t>(Setting("economy_transfer_days", 730), 1);
		run->Log("Merging daily rows older than " + std::to_string(detailDays) + " days (map: " + std::to_string(mapDays) +
			") into months; removing trades and mail older than " + std::to_string(transferDays) + " days");
		const bool queued = Background::Run("economy_compaction", [today, detailDays, mapDays, transferDays](GameDatabase& db) -> nlohmann::json {
			const auto now = static_cast<int64_t>(std::time(nullptr));
			return {
				{"merged", db.CompactEconomy(today > detailDays ? today - detailDays : 0, today > mapDays ? today - mapDays : 0)},
				{"transfers", db.PruneTransfers(now - transferDays * DAY_SECONDS)}
			};
		}, [run](nlohmann::json result, const std::string& error) {
			if (!error.empty()) return run->Finish(false, "Failed: " + error);
			run->Finish(true, "Merged " + FormatNumber(result["merged"].get<uint32_t>()) + " daily row(s) into months, removed " +
				FormatNumber(result["transfers"].get<uint32_t>()) + " old transfer(s)");
		});
		if (!queued) run->Finish(false, "Compaction is already running");
	}

	void PruneLogs(Scheduler::RunPtr run) {
		using eLog = IDashboardAdmin::eLog;
		std::vector<std::tuple<std::optional<eLog>, std::string, int64_t>> logs;
		for (const auto& [log, key, fallback] : { std::tuple{ std::optional(eLog::ACTIVITY), "log_activity_days", 365 },
			std::tuple{ std::optional(eLog::COMMAND), "log_command_days", 365 }, std::tuple{ std::optional(eLog::AUDIT), "log_audit_days", 730 },
			std::tuple{ std::optional(eLog::CHEAT_DETECTION), "log_cheat_detection_days", 365 }, std::tuple{ std::optional(eLog::CHAT), "log_chat_days", 30 },
			std::tuple{ std::optional(eLog::LOGIN_ADDRESS), "log_login_address_days", 90 },
			std::tuple{ std::optional<eLog>(), "log_task_days", 90 },
			std::tuple{ std::optional<eLog>(), "health_days", 30 },
			std::tuple{ std::optional<eLog>(), "traffic_days", 30 },
			std::tuple{ std::optional<eLog>(), "position_history_days", 3 } }) {
			const auto days = Setting(key, fallback);
			run->Log(std::string(key) + " = " + (days > 0 ? std::to_string(days) : "0 (keep everything)"));
			logs.emplace_back(log, key, days);
		}
		const bool queued = Background::Run("log_pruning", [logs](GameDatabase& db) -> nlohmann::json {
			const auto now = static_cast<int64_t>(std::time(nullptr));
			nlohmann::json pruned = nlohmann::json::object();
			for (const auto& [log, key, days] : logs) {
				if (days <= 0) continue;
				const auto before = now - days * DAY_SECONDS;
				pruned[key] = log ? db.PruneLog(*log, before) : key == std::string("health_days") ? db.PruneHealthSamples(before)
					: key == std::string("traffic_days") ? db.PruneTrafficMinutes(before)
					: key == std::string("position_history_days") ? db.PrunePositionSamples(before) : db.PruneTaskRuns(before);
			}
			return pruned;
		}, [run](nlohmann::json pruned, const std::string& error) {
			if (!error.empty()) return run->Finish(false, "Failed: " + error);
			uint32_t total = 0;
			for (const auto& [key, removed] : pruned.items()) {
				run->Log(key + ": removed " + FormatNumber(removed.get<uint32_t>()) + " row(s)");
				total += removed.get<uint32_t>();
			}
			run->Finish(true, "Removed " + FormatNumber(total) + " old log row(s)");
		});
		if (!queued) run->Finish(false, "Pruning is already running");
	}

	void CheckYesterday(Scheduler::RunPtr run) {
		const auto day = Today() - 1;
		const bool duplicates = Setting("economy_duplicate_scan", 1) != 0;
		run->Log("Checking day " + std::to_string(day) + (duplicates ? " with" : " without") + " a duplicate scan (economy_duplicate_scan)");
		RunEconomyChecks(day, duplicates, [run](const EconomyCheckResult& result) {
			if (!result.error.empty()) return run->Finish(false, result.error);
			run->Log("Unusual coin income: " + std::to_string(result.income));
			run->Log("Item spikes: " + std::to_string(result.items));
			if (result.duplicateScan) run->Log("Duplicated items: " + std::to_string(result.duplicates));
			if (result.duplicateScan) run->Log("Object ID collisions logging in will not fix: " + std::to_string(result.collisions));
			if (result.collisionsDismissed) run->Log("Dismissed " + std::to_string(result.collisionsDismissed) +
				" earlier duplicate flag(s) for object IDs shared by different items (ID collisions, not dupes)");
			run->Finish(true, std::to_string(result.Total()) + " new flag" + (result.Total() == 1 ? "" : "s"));
		});
	}
}

void RunEconomyChecks(uint32_t day, bool duplicateScan, std::function<void(const EconomyCheckResult&)> done) {
	const auto coinMultiplier = Setting("anomaly_coin_multiplier", 20), coinMinimum = Setting("anomaly_coin_minimum", 100000);
	const auto itemMultiplier = Setting("anomaly_item_multiplier", 10), itemMinimum = Setting("anomaly_item_minimum", 200);
	const bool queued = Background::Run("economy_checks", [day](GameDatabase& db) -> nlohmann::json {
		nlohmann::json income = nlohmann::json::array(), dayItems = nlohmann::json::array(), history = nlohmann::json::array();
		for (const auto& [id, gained] : db.GetDailyIncome(day)) income.push_back({ id, gained });
		for (const auto& [lot, created] : db.GetItemCreationTotals(day, day)) dayItems.push_back({ lot, created });
		for (const auto& [lot, created] : db.GetItemCreationTotals(day > HISTORY_DAYS ? day - HISTORY_DAYS : 0, day - 1)) history.push_back({ lot, created });
		return { {"income", income}, {"day", dayItems}, {"history", history} };
	}, [=](nlohmann::json raw, const std::string& error) {
		if (!error.empty()) {
			if (done) done({ .error = "The checks failed: " + error });
			return;
		}
		EconomyCheckResult result;
		result.income = FlagIncome(day, raw["income"], coinMultiplier, coinMinimum);
		result.items = FlagItems(day, raw["day"], raw["history"], itemMultiplier, itemMinimum);
		if (!duplicateScan) {
			Summarize(day, result.income, result.items, 0);
			if (done) done(result);
			return;
		}
		const bool started = StartDuplicateScan([=](const nlohmann::json& scan, const std::string& scanError) {
			auto withScan = result;
			withScan.duplicateScan = scanError.empty();
			withScan.duplicates = scanError.empty() ? FlagDuplicates(scan) : 0;
			withScan.collisionsDismissed = scanError.empty() ? DismissCollisionFlags(scan) : 0;
			withScan.collisions = scanError.empty() ? FlagCollisions(scan) : 0;
			Summarize(day, withScan.income, withScan.items, withScan.duplicates, withScan.collisions);
			if (done) done(withScan);
		});
		if (!started) {
			// Someone is running one by hand; its result is the same
			Summarize(day, result.income, result.items, 0);
			if (done) done(result);
		}
	});
	if (!queued && done) done({ .error = "The checks are already running" });
}

void RegisterEconomyTasks() {
	Scheduler::Register({ "economy_checks", "Economy checks",
		"Flags unusual coin income, item spikes and (with economy_duplicate_scan) duplicated items for the day before; the scan also "
		"dismisses open duplicate flags whose object ID turns out to be shared by different items (an ID collision from old data, not a dupe; the note "
		"says whether it resolves on login) and flags collisions that logging in will not fix (characters already migrated, or a copy in mail). "
		"Thresholds: the anomaly_* settings. The default time leaves world servers a few minutes to write the last totals.",
		"15 0 * * *", CheckYesterday, 6 * 60 * 60 });
	Scheduler::Register({ "economy_compaction", "Ledger compaction",
		"Merges daily economy rows older than economy_detail_days (map: economy_map_days) into monthly totals and deletes "
		"trades and mail older than economy_transfer_days.", "30 0 * * *", CompactLedger });
	Scheduler::Register({ "log_pruning", "Log pruning",
		"Deletes activity, command, audit, cheat detection and task run rows older than the log_*_days settings.", "45 0 * * *", PruneLogs });
}

void RegisterEconomyJobRoutes() {
	Route(eHTTPMethod::POST, "/api/reports/flags", Perm("reports_view"), "Economy flags, newest first (DataTables). Body adds {status: -1 all, 0 open, 1 dismissed, 2 actioned}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto request = ParseDataTablesRequest(context.body);
			const auto body = ParseBody(context);
			if (!request || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			auto response = Database::Get()->GetEconomyFlagsTable(request->start, std::min<uint32_t>(request->length, 200), body->value("status", 0));
			for (auto& row : response["data"]) {
				if (row.value("lot", 0) > 0) row["name"] = ClientAssets::ItemName(row["lot"].get<LOT>());
			}
			response["draw"] = request->draw;
			JsonReply(reply, eHTTPStatusCode::OK, response);
		});

	Route(eHTTPMethod::GET, "/api/reports/flags/csv", Perm("reports_view"), "Economy flags as CSV (up to 50,000). Query: ?status= (-1 all, 0 open, 1 dismissed, 2 actioned)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto status = GeneralUtils::TryParse<int32_t>(QueryValue(context.queryString, "status")).value_or(-1);
			auto data = Database::Get()->GetEconomyFlagsTable(0, 50000, status)["data"];
			static const char* KINDS[] = { "", "Coin income", "Item spike", "Duplicate", "ID collision" };
			static const char* STATUSES[] = { "Open", "Dismissed", "Actioned" };
			for (auto& row : data) {
				const auto kind = row.value("kind", 0), flagStatus = row.value("status", 0);
				row["kind_name"] = kind >= 1 && kind <= 4 ? KINDS[kind] : "";
				row["status_name"] = flagStatus >= 0 && flagStatus <= 2 ? STATUSES[flagStatus] : "";
				if (row.value("lot", 0) > 0) row["item_name"] = ClientAssets::ItemName(row["lot"].get<LOT>());
			}
			CsvReply(reply, "economy_flags.csv", ToCsv(data, {
				{"id", "ID"}, {"created_at", "Unix time"}, {"day", "Day"}, {"kind_name", "Kind"}, {"character_id", "Character ID"}, {"character_name", "Character"},
				{"lot", "LOT"}, {"item_name", "Item"}, {"item_id", "Object ID"}, {"value", "Value"}, {"baseline", "Baseline"}, {"details", "Details"},
				{"status_name", "Status"}, {"reviewer_name", "Reviewed by"}, {"note", "Note"} }));
		});

	Route(eHTTPMethod::POST, "/api/reports/flags/:id/review", Perm("reports_review_flags"), "Mark a flag reviewed. Body: {status: open|dismissed|actioned, note}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint64_t>(context.path, 3);
			const auto body = ParseBody(context);
			if (!id || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid request");
			const auto statusText = body->value("status", "");
			IDashboardAdmin::eFlagStatus status;
			if (statusText == "open") status = IDashboardAdmin::eFlagStatus::OPEN;
			else if (statusText == "dismissed") status = IDashboardAdmin::eFlagStatus::DISMISSED;
			else if (statusText == "actioned") status = IDashboardAdmin::eFlagStatus::ACTIONED;
			else return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "status must be open, dismissed or actioned");
			const std::string note = body->value("note", "");
			if (note.size() > 1000) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Note is too long");
			Database::Get()->ReviewEconomyFlag(*id, status, context.accountId, note);
			Audit(context, "review_economy_flag", "Flag " + std::to_string(*id) + " marked " + statusText + (note.empty() ? "" : ": " + note));
			BroadcastTableChanged("economy_flags", std::to_string(*id));
			JsonSuccess(reply);
		});

	Route(eHTTPMethod::POST, "/api/reports/flags/run", Perm("reports_run_checks"), "Run the anomaly checks now in the background. Body: {day (days since epoch, default yesterday), duplicates: bool}. Returns {requestId}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			if (Background::IsRunning("economy_checks")) return JsonError(reply, eHTTPStatusCode::CONFLICT, "The checks are already running");
			const auto today = Today();
			const uint32_t day = std::min<uint32_t>(body->value("day", today - 1), today);
			const auto requestId = PlayerActions::Begin(context.accountId, std::chrono::minutes(30));
			const auto actor = context;
			RunEconomyChecks(day, body->value("duplicates", false), [requestId, actor, day](const EconomyCheckResult& result) {
				if (!result.error.empty()) return PlayerActions::Finish(requestId, { false, result.error });
				const auto added = result.Total();
				Audit(actor, "run_economy_checks", "Day " + std::to_string(day) + ": " + std::to_string(added) + " new flag(s)");
				PlayerActions::Finish(requestId, { true, std::to_string(added) + " new flag" + (added == 1 ? "" : "s") });
			});
			JsonSuccess(reply, { {"requestId", requestId}, {"message", "Checks started"} });
		});
}
