#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertPositionSamples(const std::vector<PositionSample>& samples) {
	if (samples.empty()) return;
	const auto prevCommit = GetAutoCommit();
	SetAutoCommit(false);
	for (const auto& sample : samples) {
		ExecuteInsert("INSERT OR REPLACE INTO player_positions (character_id, time, zone_id, instance_id, clone_id, x, y, z) VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
			sample.characterId, sample.time, sample.zoneId, sample.instanceId, sample.cloneId, sample.x, sample.y, sample.z);
	}
	Commit();
	SetAutoCommit(prevCommit);
}

std::vector<IPlayerPositions::PositionSample> SQLiteDatabase::GetPositionSamples(uint32_t zoneId, uint32_t instanceId, int64_t from, int64_t to, int64_t bucketSeconds, uint32_t limit) {
	const std::string instance = instanceId ? " AND instance_id = ?" : "";
	const std::string query = bucketSeconds > 1
		? "SELECT character_id, instance_id, MAX(clone_id) AS clone_id, MIN(time) AS t, AVG(x) AS x, AVG(y) AS y, AVG(z) AS z FROM player_positions "
		  "WHERE zone_id = ? AND time BETWEEN ? AND ?" + instance + " GROUP BY character_id, instance_id, time / ? ORDER BY character_id, t LIMIT ?;"
		: "SELECT character_id, instance_id, clone_id, time AS t, x, y, z FROM player_positions WHERE zone_id = ? AND time BETWEEN ? AND ?" + instance +
		  " ORDER BY character_id, t LIMIT ?;";
	auto [_, result] = bucketSeconds > 1
		? (instanceId ? ExecuteSelect(query, zoneId, from, to, instanceId, bucketSeconds, limit) : ExecuteSelect(query, zoneId, from, to, bucketSeconds, limit))
		: (instanceId ? ExecuteSelect(query, zoneId, from, to, instanceId, limit) : ExecuteSelect(query, zoneId, from, to, limit));
	std::vector<PositionSample> samples;
	for (; !result.eof(); result.nextRow()) {
		samples.push_back({ result.getInt64Field("t"), result.getInt64Field("character_id"), zoneId, static_cast<uint32_t>(result.getIntField("instance_id")),
			static_cast<uint32_t>(result.getIntField("clone_id")), static_cast<float>(result.getFloatField("x")), static_cast<float>(result.getFloatField("y")),
			static_cast<float>(result.getFloatField("z")) });
	}
	return samples;
}

std::vector<IPlayerPositions::PositionInstance> SQLiteDatabase::GetPositionInstances(uint32_t zoneId, int64_t from, int64_t to) {
	const std::string query = "SELECT zone_id, instance_id, MAX(clone_id) AS clone_id, MIN(time) AS first, MAX(time) AS last, COUNT(DISTINCT character_id) AS players "
		"FROM player_positions WHERE time BETWEEN ? AND ?" + std::string(zoneId ? " AND zone_id = ?" : "") + " GROUP BY zone_id, instance_id ORDER BY last DESC LIMIT 500;";
	auto [_, result] = zoneId ? ExecuteSelect(query, from, to, zoneId) : ExecuteSelect(query, from, to);
	std::vector<PositionInstance> instances;
	for (; !result.eof(); result.nextRow()) {
		instances.push_back({ static_cast<uint32_t>(result.getIntField("zone_id")), static_cast<uint32_t>(result.getIntField("instance_id")),
			static_cast<uint32_t>(result.getIntField("clone_id")), result.getInt64Field("first"), result.getInt64Field("last"),
			static_cast<uint32_t>(result.getIntField("players")) });
	}
	return instances;
}

uint32_t SQLiteDatabase::PrunePositionSamples(int64_t beforeTime) {
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM player_positions WHERE time < ?;", beforeTime));
}

nlohmann::json SQLiteDatabase::GetMapCellsPerDay(uint32_t zoneId, std::optional<uint32_t> cloneId, uint8_t kind, uint32_t fromDay, uint32_t toDay, uint32_t limit) {
	const std::string clone = cloneId ? " AND clone_id = " + std::to_string(*cloneId) : "";
	auto [_, result] = ExecuteSelect("SELECT day, cell_x, cell_z, SUM(events) AS events FROM map_events_daily WHERE zone = ? AND kind = ? AND day BETWEEN ? AND ?" + clone +
		" GROUP BY day, cell_x, cell_z ORDER BY day, cell_x, cell_z LIMIT ?;", zoneId, static_cast<uint32_t>(kind), fromDay, toDay, limit);
	nlohmann::json rows = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"day", result.getIntField("day")}, {"x", result.getIntField("cell_x")}, {"z", result.getIntField("cell_z")}, {"events", result.getInt64Field("events")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetCloneVisitors(uint32_t zoneId, uint32_t cloneId, int64_t from, int64_t to, uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT p.character_id, COALESCE(c.name, '') AS name, MIN(p.time) AS first, MAX(p.time) AS last FROM player_positions p "
		"LEFT JOIN charinfo c ON c.id = p.character_id WHERE p.zone_id = ? AND p.clone_id = ? AND p.time BETWEEN ? AND ? "
		"GROUP BY p.character_id, c.name ORDER BY last DESC, p.character_id LIMIT ?;", zoneId, cloneId, from, to, limit);
	nlohmann::json rows = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"character_id", std::to_string(result.getInt64Field("character_id"))}, {"name", std::string(result.getStringField("name"))},
			{"first", result.getInt64Field("first")}, {"last", result.getInt64Field("last")} });
	}
	return rows;
}
