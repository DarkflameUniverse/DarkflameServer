#include "MySQLDatabase.h"

#include <chrono>

namespace {
	IUgc::ProcessInfo ReadModularProcessInfo(PreparedStmtResultSet& result, bool modular) {
		IUgc::ProcessInfo info;
		info.id = result->getInt64("id");
		info.characterId = result->getInt64("character_id");
		info.characterName = std::string(result->getString("character_name").c_str());
		info.state = static_cast<IUgc::eProcessState>(result->getInt("is_optimized"));
		info.attempts = static_cast<uint32_t>(result->getInt("process_attempts"));
		info.processedAt = result->getInt64("processed_at");
		info.error = std::string(result->getString("process_error").c_str());
		if (modular) info.details = std::string(result->getString("ldf_config").c_str());
		else info.bakeAo = result->getInt("bake_ao") != 0;
		return info;
	}

	int64_t UnixNow() {
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
}

void MySQLDatabase::InsertUgcBuild(const std::string& modules, const LWOOBJID bigId, const std::optional<LWOOBJID> characterId) {
	ExecuteInsert("INSERT INTO ugc_modular_build (ugc_id, ldf_config, character_id) VALUES (?,?,?)", bigId, modules, characterId);
}

void MySQLDatabase::DeleteUgcBuild(const LWOOBJID bigId) {
	ExecuteDelete("DELETE FROM ugc_modular_build WHERE ugc_id = ?;", bigId);
}

std::vector<IUgcModularBuild::PendingBuild> MySQLDatabase::GetModularBuildsToProcess(const uint32_t limit) {
	auto result = ExecuteSelect("SELECT ugc_id, ldf_config, process_attempts FROM ugc_modular_build WHERE is_optimized = 0 ORDER BY process_attempts ASC, ugc_id DESC LIMIT ?;", limit);
	std::vector<PendingBuild> builds;
	while (result->next()) {
		builds.push_back({ result->getInt64("ugc_id"), std::string(result->getString("ldf_config").c_str()), static_cast<uint32_t>(result->getInt("process_attempts")) });
	}
	return builds;
}

void MySQLDatabase::SetModularBuildProcessed(const LWOOBJID id, const IUgc::eProcessState state, const uint32_t attempts, const std::string_view error) {
	ExecuteUpdate("UPDATE ugc_modular_build SET is_optimized = ?, process_attempts = ?, process_error = ?, processed_at = ? WHERE ugc_id = ?;",
		static_cast<int32_t>(state), attempts, error, UnixNow(), id);
}

namespace {
	const std::string MODULAR_SELECT =
		"SELECT b.ugc_id AS id, b.character_id, c.name AS character_name, b.is_optimized, b.process_attempts, b.processed_at, b.process_error, b.ldf_config "
		"FROM ugc_modular_build AS b LEFT JOIN charinfo AS c ON c.id = b.character_id ";
}

std::optional<IUgc::ProcessInfo> MySQLDatabase::GetModularBuildProcessInfo(const LWOOBJID id) {
	auto result = ExecuteSelect(MODULAR_SELECT + "WHERE b.ugc_id = ? LIMIT 1;", id);
	if (!result->next()) return std::nullopt;
	return ReadModularProcessInfo(result, true);
}

uint64_t MySQLDatabase::ResetModularBuildProcessing(const std::optional<LWOOBJID> id, const bool failedOnly) {
	if (id) return ExecuteUpdate("UPDATE ugc_modular_build SET is_optimized = 0, process_attempts = 0, process_error = '' WHERE ugc_id = ?;", *id);
	if (failedOnly) return ExecuteUpdate("UPDATE ugc_modular_build SET is_optimized = 0, process_attempts = 0, process_error = '' WHERE is_optimized = 2;");
	return ExecuteUpdate("UPDATE ugc_modular_build SET is_optimized = 0, process_attempts = 0, process_error = '';");
}

std::vector<IUgc::ProcessInfo> MySQLDatabase::GetModularBuildProcessList(const std::optional<IUgc::eProcessState> state, const std::string_view search, const uint32_t offset, const uint32_t limit) {
	std::vector<IUgc::ProcessInfo> list;
	// Every filter always bound, off when its first value says so
	const int32_t stateValue = state ? static_cast<int32_t>(*state) : -1;
	const std::string text(search);
	const std::string pattern = "%" + text + "%";
	const std::string where = "WHERE (? < 0 OR b.is_optimized = ?) AND (? = '' OR CAST(b.ugc_id AS CHAR) = ? OR c.name LIKE ?) ";
	auto result = ExecuteSelect(MODULAR_SELECT + where + "ORDER BY b.ugc_id DESC LIMIT ? OFFSET ?;", stateValue, stateValue, text, text, pattern, limit, offset);
	while (result->next()) {
		list.push_back(ReadModularProcessInfo(result, true));
	}
	return list;
}

std::vector<std::pair<IUgc::eProcessState, uint64_t>> MySQLDatabase::GetModularBuildProcessCounts() {
	auto result = ExecuteSelect("SELECT is_optimized, COUNT(*) AS count FROM ugc_modular_build GROUP BY is_optimized;");
	std::vector<std::pair<IUgc::eProcessState, uint64_t>> counts;
	while (result->next()) {
		counts.emplace_back(static_cast<IUgc::eProcessState>(result->getInt("is_optimized")), static_cast<uint64_t>(result->getInt64("count")));
	}
	return counts;
}

std::vector<std::pair<std::string, uint64_t>> MySQLDatabase::GetModularBuildConfigCounts() {
	auto result = ExecuteSelect("SELECT ldf_config, COUNT(*) AS count FROM ugc_modular_build GROUP BY ldf_config;");
	std::vector<std::pair<std::string, uint64_t>> counts;
	while (result->next()) counts.emplace_back(result->getString("ldf_config").c_str(), static_cast<uint64_t>(result->getInt64("count")));
	return counts;
}

std::optional<std::string> MySQLDatabase::GetUgcIconSettings(const std::string_view target) {
	auto result = ExecuteSelect("SELECT params FROM ugc_icon_settings WHERE target = ? LIMIT 1;", target);
	if (!result->next()) return std::nullopt;
	return std::string(result->getString("params").c_str());
}

void MySQLDatabase::SetUgcIconSettings(const std::string_view target, const std::string_view params) {
	ExecuteInsert("INSERT INTO ugc_icon_settings (target, params, updated_at) VALUES (?, ?, ?) "
		"ON DUPLICATE KEY UPDATE params = VALUES(params), updated_at = VALUES(updated_at);", target, params, UnixNow());
}

void MySQLDatabase::DeleteUgcIconSettings(const std::string_view target) {
	ExecuteDelete("DELETE FROM ugc_icon_settings WHERE target = ?;", target);
}

void MySQLDatabase::SetModularBuildCombination(const LWOOBJID id, const LWOOBJID combinationId) {
	ExecuteUpdate("UPDATE ugc_modular_build SET combination_id = ? WHERE ugc_id = ?;", combinationId, id);
}

std::vector<IUgcModularBuild::PendingBuild> MySQLDatabase::GetModularBuildsWithoutCombination(const uint32_t limit) {
	auto result = ExecuteSelect("SELECT ugc_id, ldf_config, process_attempts FROM ugc_modular_build WHERE combination_id = 0 LIMIT ?;", limit);
	std::vector<PendingBuild> builds;
	while (result->next()) {
		builds.push_back({ result->getInt64("ugc_id"), std::string(result->getString("ldf_config").c_str()), static_cast<uint32_t>(result->getInt("process_attempts")) });
	}
	return builds;
}
