#include "SQLiteDatabase.h"

#include <chrono>

namespace {
	IUgc::ProcessInfo ReadModularProcessInfo(CppSQLite3Query& result, bool modular) {
		IUgc::ProcessInfo info;
		info.id = result.getInt64Field("id");
		info.characterId = result.getInt64Field("character_id");
		info.characterName = result.getStringField("character_name", "");
		info.state = static_cast<IUgc::eProcessState>(result.getIntField("is_optimized"));
		info.attempts = static_cast<uint32_t>(result.getIntField("process_attempts"));
		info.processedAt = result.getInt64Field("processed_at");
		info.error = result.getStringField("process_error", "");
		if (modular) info.details = result.getStringField("ldf_config", "");
		else info.bakeAo = result.getIntField("bake_ao") != 0;
		return info;
	}

	int64_t UnixNow() {
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
}

void SQLiteDatabase::InsertUgcBuild(const std::string& modules, const LWOOBJID bigId, const std::optional<LWOOBJID> characterId) {
	ExecuteInsert("INSERT INTO ugc_modular_build (ugc_id, ldf_config, character_id) VALUES (?,?,?)", bigId, modules, characterId);
}

void SQLiteDatabase::DeleteUgcBuild(const LWOOBJID bigId) {
	ExecuteDelete("DELETE FROM ugc_modular_build WHERE ugc_id = ?;", bigId);
}

std::vector<IUgcModularBuild::PendingBuild> SQLiteDatabase::GetModularBuildsToProcess(const uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT ugc_id, ldf_config, process_attempts FROM ugc_modular_build WHERE is_optimized = 0 ORDER BY process_attempts ASC, ugc_id DESC LIMIT ?;", limit);
	std::vector<PendingBuild> builds;
	while (!result.eof()) {
		builds.push_back({ result.getInt64Field("ugc_id"), result.getStringField("ldf_config", ""), static_cast<uint32_t>(result.getIntField("process_attempts")) });
		result.nextRow();
	}
	return builds;
}

void SQLiteDatabase::SetModularBuildProcessed(const LWOOBJID id, const IUgc::eProcessState state, const uint32_t attempts, const std::string_view error) {
	ExecuteUpdate("UPDATE ugc_modular_build SET is_optimized = ?, process_attempts = ?, process_error = ?, processed_at = ? WHERE ugc_id = ?;",
		static_cast<int32_t>(state), attempts, error, UnixNow(), id);
}

namespace {
	const std::string MODULAR_SELECT =
		"SELECT b.ugc_id AS id, b.character_id, c.name AS character_name, b.is_optimized, b.process_attempts, b.processed_at, b.process_error, b.ldf_config "
		"FROM ugc_modular_build AS b LEFT JOIN charinfo AS c ON c.id = b.character_id ";
}

std::optional<IUgc::ProcessInfo> SQLiteDatabase::GetModularBuildProcessInfo(const LWOOBJID id) {
	auto [_, result] = ExecuteSelect(MODULAR_SELECT + "WHERE b.ugc_id = ? LIMIT 1;", id);
	if (result.eof()) return std::nullopt;
	return ReadModularProcessInfo(result, true);
}

uint64_t SQLiteDatabase::ResetModularBuildProcessing(const std::optional<LWOOBJID> id, const bool failedOnly) {
	if (id) return ExecuteUpdate("UPDATE ugc_modular_build SET is_optimized = 0, process_attempts = 0, process_error = '' WHERE ugc_id = ?;", *id);
	if (failedOnly) return ExecuteUpdate("UPDATE ugc_modular_build SET is_optimized = 0, process_attempts = 0, process_error = '' WHERE is_optimized = 2;");
	return ExecuteUpdate("UPDATE ugc_modular_build SET is_optimized = 0, process_attempts = 0, process_error = '';");
}

std::vector<IUgc::ProcessInfo> SQLiteDatabase::GetModularBuildProcessList(const std::optional<IUgc::eProcessState> state, const std::string_view search, const uint32_t offset, const uint32_t limit) {
	std::vector<IUgc::ProcessInfo> list;
	// Every filter always bound, off when its first value says so
	const int32_t stateValue = state ? static_cast<int32_t>(*state) : -1;
	const std::string text(search);
	const std::string pattern = "%" + text + "%";
	const std::string where = "WHERE (? < 0 OR b.is_optimized = ?) AND (? = '' OR CAST(b.ugc_id AS CHAR) = ? OR c.name LIKE ?) ";
	auto [_, result] = ExecuteSelect(MODULAR_SELECT + where + "ORDER BY b.ugc_id DESC LIMIT ? OFFSET ?;", stateValue, stateValue, text, text, pattern, limit, offset);
	while (!result.eof()) {
		list.push_back(ReadModularProcessInfo(result, true));
		result.nextRow();
	}
	return list;
}

std::vector<std::pair<IUgc::eProcessState, uint64_t>> SQLiteDatabase::GetModularBuildProcessCounts() {
	auto [_, result] = ExecuteSelect("SELECT is_optimized, COUNT(*) AS count FROM ugc_modular_build GROUP BY is_optimized;");
	std::vector<std::pair<IUgc::eProcessState, uint64_t>> counts;
	while (!result.eof()) {
		counts.emplace_back(static_cast<IUgc::eProcessState>(result.getIntField("is_optimized")), static_cast<uint64_t>(result.getInt64Field("count")));
		result.nextRow();
	}
	return counts;
}

std::vector<std::pair<std::string, uint64_t>> SQLiteDatabase::GetModularBuildConfigCounts() {
	auto [_, result] = ExecuteSelect("SELECT ldf_config, COUNT(*) AS count FROM ugc_modular_build GROUP BY ldf_config;");
	std::vector<std::pair<std::string, uint64_t>> counts;
	while (!result.eof()) {
		counts.emplace_back(result.getStringField("ldf_config", ""), static_cast<uint64_t>(result.getInt64Field("count")));
		result.nextRow();
	}
	return counts;
}

std::optional<std::string> SQLiteDatabase::GetUgcIconSettings(const std::string_view target) {
	auto [_, result] = ExecuteSelect("SELECT params FROM ugc_icon_settings WHERE target = ? LIMIT 1;", target);
	if (result.eof()) return std::nullopt;
	return std::string(result.getStringField("params", ""));
}

void SQLiteDatabase::SetUgcIconSettings(const std::string_view target, const std::string_view params) {
	ExecuteInsert("INSERT INTO ugc_icon_settings (target, params, updated_at) VALUES (?, ?, ?) "
		"ON CONFLICT(target) DO UPDATE SET params = excluded.params, updated_at = excluded.updated_at;", target, params, UnixNow());
}

void SQLiteDatabase::DeleteUgcIconSettings(const std::string_view target) {
	ExecuteDelete("DELETE FROM ugc_icon_settings WHERE target = ?;", target);
}

void SQLiteDatabase::SetModularBuildCombination(const LWOOBJID id, const LWOOBJID combinationId) {
	ExecuteUpdate("UPDATE ugc_modular_build SET combination_id = ? WHERE ugc_id = ?;", combinationId, id);
}

std::vector<IUgcModularBuild::PendingBuild> SQLiteDatabase::GetModularBuildsWithoutCombination(const uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT ugc_id, ldf_config, process_attempts FROM ugc_modular_build WHERE combination_id = 0 LIMIT ?;", limit);
	std::vector<PendingBuild> builds;
	while (!result.eof()) {
		builds.push_back({ result.getInt64Field("ugc_id"), result.getStringField("ldf_config", ""), static_cast<uint32_t>(result.getIntField("process_attempts")) });
		result.nextRow();
	}
	return builds;
}

void SQLiteDatabase::SetModularBuildProcessStats(const LWOOBJID id, const IUgc::ProcessStats& stats) {
	ExecuteUpdate("UPDATE ugc_modular_build SET process_ms = ?, process_cpu_ms = ?, process_memory_kb = ? WHERE ugc_id = ?;", stats.milliseconds, stats.cpuMilliseconds, stats.memoryKb, id);
}
