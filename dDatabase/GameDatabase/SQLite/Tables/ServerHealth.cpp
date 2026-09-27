#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertHealthSample(const HealthSample& sample) {
	ExecuteInsert("INSERT OR REPLACE INTO server_health (time, players, worlds, auth_online, chat_online, memory_kb, ugc_enabled, ugc_online) VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
		sample.time, sample.players, sample.worlds, sample.authOnline, sample.chatOnline, static_cast<int64_t>(sample.memoryKb), sample.ugcEnabled, sample.ugcOnline);
}

std::vector<IServerHealth::HealthSample> SQLiteDatabase::GetHealthSamples(int64_t from, int64_t to, int64_t bucketSeconds) {
	std::vector<HealthSample> samples;
	auto [_, result] = ExecuteSelect("SELECT (time / ?) * ? AS bucket, MAX(players) AS players, MAX(worlds) AS worlds, MIN(auth_online) AS auth, MIN(chat_online) AS chat, AVG(memory_kb) AS memory, MAX(ugc_enabled) AS ugc_enabled, MIN(ugc_online) AS ugc "
		"FROM server_health WHERE time >= ? AND time <= ? GROUP BY bucket ORDER BY bucket;", bucketSeconds, bucketSeconds, from, to);
	while (!result.eof()) {
		samples.push_back({ result.getInt64Field("bucket"), static_cast<uint32_t>(result.getIntField("players")), static_cast<uint32_t>(result.getIntField("worlds")),
			result.getIntField("auth") != 0, result.getIntField("chat") != 0, static_cast<uint64_t>(result.getFloatField("memory")),
			result.getIntField("ugc_enabled") != 0, result.getIntField("ugc") != 0 });
		result.nextRow();
	}
	return samples;
}

uint32_t SQLiteDatabase::PruneHealthSamples(int64_t beforeTime) {
	ExecuteDelete("DELETE FROM server_health_instances WHERE time < ?;", beforeTime);
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM server_health WHERE time < ?;", beforeTime));
}

void SQLiteDatabase::InsertInstanceSamples(const std::vector<InstanceSample>& samples) {
	for (const auto& sample : samples) {
		ExecuteInsert("INSERT OR REPLACE INTO server_health_instances (time, zone_id, instance_id, clone_id, players) VALUES (?, ?, ?, ?, ?);",
			sample.time, sample.zoneId, sample.instanceId, sample.cloneId, sample.players);
	}
}

std::vector<IServerHealth::InstanceSample> SQLiteDatabase::GetInstanceSamples(int64_t from, int64_t to, int64_t bucketSeconds, uint32_t zoneId) {
	std::vector<InstanceSample> samples;
	const std::string query = "SELECT (time / ?) * ? AS bucket, zone_id, instance_id, MAX(clone_id) AS clone_id, MAX(players) AS players FROM server_health_instances "
		"WHERE time >= ? AND time <= ?" + std::string(zoneId ? " AND zone_id = ?" : "") + " GROUP BY bucket, zone_id, instance_id ORDER BY bucket, zone_id, instance_id;";
	auto [_, result] = zoneId ? ExecuteSelect(query, bucketSeconds, bucketSeconds, from, to, zoneId) : ExecuteSelect(query, bucketSeconds, bucketSeconds, from, to);
	for (; !result.eof(); result.nextRow()) {
		samples.push_back({ result.getInt64Field("bucket"), static_cast<uint32_t>(result.getIntField("zone_id")), static_cast<uint32_t>(result.getIntField("instance_id")),
			static_cast<uint32_t>(result.getIntField("clone_id")), static_cast<uint32_t>(result.getIntField("players")) });
	}
	return samples;
}
