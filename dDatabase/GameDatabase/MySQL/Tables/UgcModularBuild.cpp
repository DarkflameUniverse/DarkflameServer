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

std::vector<IUgc::ProcessInfo> MySQLDatabase::GetModularBuildProcessList(const std::optional<IUgc::eProcessState> state, const uint32_t offset, const uint32_t limit) {
	std::vector<IUgc::ProcessInfo> list;
	auto read = [&list](PreparedStmtResultSet& result) {
		while (result->next()) {
			list.push_back(ReadModularProcessInfo(result, true));
		}
	};
	if (state) {
		auto result = ExecuteSelect(MODULAR_SELECT + "WHERE b.is_optimized = ? ORDER BY b.ugc_id DESC LIMIT ? OFFSET ?;", static_cast<int32_t>(*state), limit, offset);
		read(result);
	} else {
		auto result = ExecuteSelect(MODULAR_SELECT + "ORDER BY b.ugc_id DESC LIMIT ? OFFSET ?;", limit, offset);
		read(result);
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
