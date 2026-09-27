#include "SQLiteDatabase.h"

#include <chrono>

namespace {
	IUgc::ProcessInfo ReadUgcProcessInfo(CppSQLite3Query& result, bool modular) {
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

IUgc::Model ReadModel(CppSQLite3Query& result) {
	IUgc::Model model;

	int blobSize{};
	const auto* blob = result.getBlobField("lxfml", blobSize);
	model.lxfmlData << std::string(reinterpret_cast<const char*>(blob), blobSize);
	model.id = result.getInt64Field("ugcID");
	model.modelID = result.getInt64Field("modelID");

	return model;
}

std::vector<IUgc::Model> SQLiteDatabase::GetUgcModels(const LWOOBJID& propertyId) {
	auto [_, result] = ExecuteSelect(
		"SELECT lxfml, u.id AS ugcID, pc.id AS modelID FROM ugc AS u JOIN properties_contents AS pc ON u.id = pc.ugc_id WHERE lot = 14 AND property_id = ? AND pc.ugc_id IS NOT NULL;",
		propertyId);

	std::vector<IUgc::Model> toReturn;

	while (!result.eof()) {
		toReturn.push_back(ReadModel(result));
		result.nextRow();
	}

	return toReturn;
}

std::vector<IUgc::Model> SQLiteDatabase::GetAllUgcModels() {
	auto [_, result] = ExecuteSelect("SELECT u.id AS ugcID, pc.id AS modelID, lxfml FROM ugc AS u JOIN properties_contents AS pc ON pc.ugc_id = u.id WHERE pc.lot = 14 AND pc.ugc_id IS NOT NULL;");

	std::vector<IUgc::Model> models;
	while (!result.eof()) {
		models.push_back(ReadModel(result));
		result.nextRow();
	}

	return models;
}

void SQLiteDatabase::RemoveUnreferencedUgcModels() {
	ExecuteDelete("DELETE FROM ugc WHERE id NOT IN (SELECT ugc_id FROM properties_contents WHERE ugc_id IS NOT NULL);");
}

void SQLiteDatabase::InsertNewUgcModel(
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

void SQLiteDatabase::DeleteUgcModelData(const LWOOBJID& modelId) {
	ExecuteDelete("DELETE FROM ugc WHERE id = ?;", modelId);
	ExecuteDelete("DELETE FROM properties_contents WHERE ugc_id = ?;", modelId);
}

void SQLiteDatabase::UpdateUgcModelData(const LWOOBJID& modelId, std::stringstream& lxfml) {
	const std::istream stream(lxfml.rdbuf());
	// The UGC server makes the model's files again
	ExecuteUpdate("UPDATE ugc SET lxfml = ?, is_optimized = 0, process_attempts = 0, process_error = '' WHERE id = ?;", &stream, modelId);
}

std::optional<IUgc::Model> SQLiteDatabase::GetUgcModel(const LWOOBJID ugcId) {
	// LEFT JOIN: a model that is in someone's inventory rather than placed on a property still exists
	auto [_, result] = ExecuteSelect("SELECT u.id AS ugcID, pc.id AS modelID, lxfml FROM ugc AS u LEFT JOIN properties_contents AS pc ON pc.ugc_id = u.id WHERE u.id = ? LIMIT 1;", ugcId);

	std::optional<IUgc::Model> toReturn = std::nullopt;
	if (!result.eof()) {
		toReturn = ReadModel(result);
	}

	return toReturn;
}

std::vector<IUgc::PendingModel> SQLiteDatabase::GetUgcModelsToProcess(const uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT id, lxfml, process_attempts FROM ugc WHERE is_optimized = 0 ORDER BY process_attempts ASC, id DESC LIMIT ?;", limit);
	std::vector<IUgc::PendingModel> models;
	while (!result.eof()) {
		auto& model = models.emplace_back();
		model.id = result.getInt64Field("id");
		int blobSize{};
		const auto* blob = result.getBlobField("lxfml", blobSize);
		model.lxfml.assign(reinterpret_cast<const char*>(blob), blobSize);
		model.attempts = static_cast<uint32_t>(result.getIntField("process_attempts"));
		result.nextRow();
	}
	return models;
}

void SQLiteDatabase::SetUgcModelProcessed(const LWOOBJID id, const eProcessState state, const uint32_t attempts, const std::string_view error, const bool bakeAo) {
	ExecuteUpdate("UPDATE ugc SET is_optimized = ?, process_attempts = ?, process_error = ?, bake_ao = ?, processed_at = ? WHERE id = ?;",
		static_cast<int32_t>(state), attempts, error, bakeAo, UnixNow(), id);
}

std::optional<IUgc::ProcessInfo> SQLiteDatabase::GetUgcProcessInfo(const LWOOBJID id) {
	auto [_, result] = ExecuteSelect(
		"SELECT u.id, u.character_id, c.name AS character_name, u.is_optimized, u.process_attempts, u.processed_at, u.process_error, u.bake_ao "
		"FROM ugc AS u LEFT JOIN charinfo AS c ON c.id = u.character_id WHERE u.id = ? LIMIT 1;", id);
	if (result.eof()) return std::nullopt;
	return ReadUgcProcessInfo(result, false);
}

uint64_t SQLiteDatabase::ResetUgcModelProcessing(const std::optional<LWOOBJID> id, const bool failedOnly) {
	if (id) return ExecuteUpdate("UPDATE ugc SET is_optimized = 0, process_attempts = 0, process_error = '' WHERE id = ?;", *id);
	if (failedOnly) return ExecuteUpdate("UPDATE ugc SET is_optimized = 0, process_attempts = 0, process_error = '' WHERE is_optimized = 2;");
	return ExecuteUpdate("UPDATE ugc SET is_optimized = 0, process_attempts = 0, process_error = '';");
}

std::vector<IUgc::ProcessInfo> SQLiteDatabase::GetUgcProcessList(const std::optional<eProcessState> state, const std::string_view search, const uint32_t offset, const uint32_t limit) {
	const std::string select =
		"SELECT u.id, u.character_id, c.name AS character_name, u.is_optimized, u.process_attempts, u.processed_at, u.process_error, u.bake_ao "
		"FROM ugc AS u LEFT JOIN charinfo AS c ON c.id = u.character_id ";
	std::vector<IUgc::ProcessInfo> list;
	// Every filter always bound, off when its first value says so
	const int32_t stateValue = state ? static_cast<int32_t>(*state) : -1;
	const std::string text(search);
	const std::string pattern = "%" + text + "%";
	const std::string where = "WHERE (? < 0 OR u.is_optimized = ?) AND (? = '' OR CAST(u.id AS CHAR) = ? OR c.name LIKE ?) ";
	auto [_, result] = ExecuteSelect(select + where + "ORDER BY u.id DESC LIMIT ? OFFSET ?;", stateValue, stateValue, text, text, pattern, limit, offset);
	while (!result.eof()) {
		list.push_back(ReadUgcProcessInfo(result, false));
		result.nextRow();
	}
	return list;
}

std::vector<std::pair<IUgc::eProcessState, uint64_t>> SQLiteDatabase::GetUgcProcessCounts() {
	auto [_, result] = ExecuteSelect("SELECT is_optimized, COUNT(*) AS count FROM ugc GROUP BY is_optimized;");
	std::vector<std::pair<eProcessState, uint64_t>> counts;
	while (!result.eof()) {
		counts.emplace_back(static_cast<eProcessState>(result.getIntField("is_optimized")), static_cast<uint64_t>(result.getInt64Field("count")));
		result.nextRow();
	}
	return counts;
}
