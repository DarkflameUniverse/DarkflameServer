#include "MySQLDatabase.h"

#include <chrono>

namespace {
	IUgc::ProcessInfo ReadUgcProcessInfo(PreparedStmtResultSet& result, bool modular) {
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

IUgc::Model ReadModel(PreparedStmtResultSet& result) {
	IUgc::Model model;

	// blob is owned by the query, so we need to do a deep copy :/
	std::unique_ptr<std::istream> blob(result->getBlob("lxfml"));
	model.lxfmlData << blob->rdbuf();
	model.id = result->getUInt64("ugcID");
	model.modelID = result->getUInt64("modelID");

	return model;
}

std::vector<IUgc::Model> MySQLDatabase::GetUgcModels(const LWOOBJID& propertyId) {
	auto result = ExecuteSelect(
		"SELECT lxfml, u.id as ugcID, pc.id as modelID FROM ugc AS u JOIN properties_contents AS pc ON u.id = pc.ugc_id WHERE lot = 14 AND property_id = ? AND pc.ugc_id IS NOT NULL;",
		propertyId);

	std::vector<IUgc::Model> toReturn;

	while (result->next()) {
		toReturn.push_back(ReadModel(result));
	}

	return toReturn;
}

std::vector<IUgc::Model> MySQLDatabase::GetAllUgcModels() {
	auto result = ExecuteSelect("SELECT u.id AS ugcID, lxfml, pc.id AS modelID FROM ugc AS u JOIN properties_contents AS pc ON pc.ugc_id = u.id WHERE pc.lot = 14 AND pc.ugc_id IS NOT NULL;");

	std::vector<IUgc::Model> models;
	models.reserve(result->rowsCount());
	while (result->next()) {
		models.push_back(ReadModel(result));
	}

	return models;
}

void MySQLDatabase::RemoveUnreferencedUgcModels() {
	ExecuteDelete("DELETE FROM ugc WHERE id NOT IN (SELECT ugc_id FROM properties_contents WHERE ugc_id IS NOT NULL);");
}

void MySQLDatabase::InsertNewUgcModel(
	std::stringstream& sd0Data, // cant be const sad
	const uint64_t blueprintId,
	const uint32_t accountId,
	const LWOOBJID characterId) {
	const std::istream stream(sd0Data.rdbuf());
	ExecuteInsert(
		"INSERT INTO `ugc`(`id`, `account_id`, `character_id`, `is_optimized`, `lxfml`, `bake_ao`, `filename`) VALUES (?,?,?,?,?,?,?)",
		blueprintId,
		accountId,
		characterId,
		0,
		&stream,
		false,
		"weedeater.lxfml"
	);
}

void MySQLDatabase::DeleteUgcModelData(const LWOOBJID& modelId) {
	ExecuteDelete("DELETE FROM ugc WHERE id = ?;", modelId);
	ExecuteDelete("DELETE FROM properties_contents WHERE ugc_id = ?;", modelId);
}

void MySQLDatabase::UpdateUgcModelData(const LWOOBJID& modelId, std::stringstream& lxfml) {
	const std::istream stream(lxfml.rdbuf());
	// The UGC server makes the model's files again
	ExecuteUpdate("UPDATE ugc SET lxfml = ?, is_optimized = 0, process_attempts = 0, process_error = '' WHERE id = ?;", &stream, modelId);
}

std::optional<IUgc::Model> MySQLDatabase::GetUgcModel(const LWOOBJID ugcId) {
	// LEFT JOIN: a model that is in someone's inventory rather than placed on a property still exists
	auto result = ExecuteSelect("SELECT u.id AS ugcID, lxfml, pc.id AS modelID FROM ugc AS u LEFT JOIN properties_contents AS pc ON pc.ugc_id = u.id WHERE u.id = ? LIMIT 1;", ugcId);

	std::optional<IUgc::Model> toReturn = std::nullopt;
	if (result->next()) {
		toReturn = ReadModel(result);
	}

	return toReturn;
}

std::vector<IUgc::PendingModel> MySQLDatabase::GetUgcModelsToProcess(const uint32_t limit) {
	auto result = ExecuteSelect("SELECT id, lxfml, process_attempts FROM ugc WHERE is_optimized = 0 ORDER BY process_attempts ASC, id DESC LIMIT ?;", limit);
	std::vector<IUgc::PendingModel> models;
	while (result->next()) {
		auto& model = models.emplace_back();
		model.id = result->getInt64("id");
		std::unique_ptr<std::istream> blob(result->getBlob("lxfml"));
		std::stringstream contents;
		contents << blob->rdbuf();
		model.lxfml = contents.str();
		model.attempts = static_cast<uint32_t>(result->getInt("process_attempts"));
	}
	return models;
}

void MySQLDatabase::SetUgcModelProcessed(const LWOOBJID id, const eProcessState state, const uint32_t attempts, const std::string_view error, const bool bakeAo) {
	ExecuteUpdate("UPDATE ugc SET is_optimized = ?, process_attempts = ?, process_error = ?, bake_ao = ?, processed_at = ? WHERE id = ?;",
		static_cast<int32_t>(state), attempts, error, bakeAo, UnixNow(), id);
}

std::optional<IUgc::ProcessInfo> MySQLDatabase::GetUgcProcessInfo(const LWOOBJID id) {
	auto result = ExecuteSelect(
		"SELECT u.id, u.character_id, c.name AS character_name, u.is_optimized, u.process_attempts, u.processed_at, u.process_error, u.bake_ao "
		"FROM ugc AS u LEFT JOIN charinfo AS c ON c.id = u.character_id WHERE u.id = ? LIMIT 1;", id);
	if (!result->next()) return std::nullopt;
	return ReadUgcProcessInfo(result, false);
}

uint64_t MySQLDatabase::ResetUgcModelProcessing(const std::optional<LWOOBJID> id, const bool failedOnly) {
	if (id) return ExecuteUpdate("UPDATE ugc SET is_optimized = 0, process_attempts = 0, process_error = '' WHERE id = ?;", *id);
	if (failedOnly) return ExecuteUpdate("UPDATE ugc SET is_optimized = 0, process_attempts = 0, process_error = '' WHERE is_optimized = 2;");
	return ExecuteUpdate("UPDATE ugc SET is_optimized = 0, process_attempts = 0, process_error = '';");
}

std::vector<IUgc::ProcessInfo> MySQLDatabase::GetUgcProcessList(const std::optional<eProcessState> state, const uint32_t offset, const uint32_t limit) {
	const std::string select =
		"SELECT u.id, u.character_id, c.name AS character_name, u.is_optimized, u.process_attempts, u.processed_at, u.process_error, u.bake_ao "
		"FROM ugc AS u LEFT JOIN charinfo AS c ON c.id = u.character_id ";
	std::vector<IUgc::ProcessInfo> list;
	auto read = [&list](PreparedStmtResultSet& result) {
		while (result->next()) {
			list.push_back(ReadUgcProcessInfo(result, false));
		}
	};
	if (state) {
		auto result = ExecuteSelect(select + "WHERE u.is_optimized = ? ORDER BY u.id DESC LIMIT ? OFFSET ?;", static_cast<int32_t>(*state), limit, offset);
		read(result);
	} else {
		auto result = ExecuteSelect(select + "ORDER BY u.id DESC LIMIT ? OFFSET ?;", limit, offset);
		read(result);
	}
	return list;
}

std::vector<std::pair<IUgc::eProcessState, uint64_t>> MySQLDatabase::GetUgcProcessCounts() {
	auto result = ExecuteSelect("SELECT is_optimized, COUNT(*) AS count FROM ugc GROUP BY is_optimized;");
	std::vector<std::pair<eProcessState, uint64_t>> counts;
	while (result->next()) {
		counts.emplace_back(static_cast<eProcessState>(result->getInt("is_optimized")), static_cast<uint64_t>(result->getInt64("count")));
	}
	return counts;
}
