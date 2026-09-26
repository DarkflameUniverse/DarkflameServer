#include "SQLiteDatabase.h"

#include "GeneralUtils.h"

namespace {
	uint64_t U64(CppSQLite3Query& result, const char* field) { return static_cast<uint64_t>(result.getInt64Field(field)); }
	uint32_t U32(CppSQLite3Query& result, const char* field) { return static_cast<uint32_t>(result.getInt64Field(field)); }
	std::string JoinZones(const std::vector<uint32_t>& zones) {
		std::string text;
		for (const auto zone : zones) text += (text.empty() ? "" : ",") + std::to_string(zone);
		return text;
	}

	std::vector<uint32_t> SplitZones(const std::string& text) {
		std::vector<uint32_t> zones;
		for (const auto& part : GeneralUtils::SplitString(text, ',')) {
			if (const auto zone = GeneralUtils::TryParse<uint32_t>(part)) zones.push_back(*zone);
		}
		return zones;
	}

	std::string Text(CppSQLite3Query& result, const char* field) {
		return result.fieldIsNull(field) ? "" : std::string(result.getStringField(field));
	}

	ILiveOps::LiveEvent ReadEvent(CppSQLite3Query& result) {
		ILiveOps::LiveEvent row;
		row.id = U64(result, "id");
		row.type = Text(result, "type");
		row.title = Text(result, "title");
		row.message = Text(result, "message");
		row.zones = SplitZones(Text(result, "zones"));
		row.instanceId = result.getIntField("instance_id");
		row.config = Text(result, "config");
		row.startsAt = result.getInt64Field("starts_at");
		row.endsAt = result.getInt64Field("ends_at");
		row.state = static_cast<ILiveOps::eLiveEventState>(result.getIntField("state"));
		row.endedAt = result.getInt64Field("ended_at");
		row.endReason = Text(result, "end_reason");
		row.createdBy = Text(result, "created_by");
		row.endedBy = Text(result, "ended_by");
		return row;
	}

	ILiveOps::Challenge ReadChallenge(CppSQLite3Query& result) {
		ILiveOps::Challenge row;
		row.id = U64(result, "id");
		row.title = Text(result, "title");
		row.description = Text(result, "description");
		row.metricKind = static_cast<ILiveOps::eChallengeMetric>(result.getIntField("metric_kind"));
		row.metric = U32(result, "metric");
		row.lot = result.getIntField("lot");
		row.zones = SplitZones(Text(result, "zones"));
		row.target = result.getInt64Field("target");
		row.startsAt = result.getInt64Field("starts_at");
		row.endsAt = result.getInt64Field("ends_at");
		row.includeStaff = result.getIntField("include_staff") != 0;
		row.isPublic = result.getIntField("is_public") != 0;
		row.rewardCoins = result.getInt64Field("reward_coins");
		row.rewardItems = Text(result, "reward_items");
		row.rewardMin = result.getInt64Field("reward_min");
		row.state = static_cast<ILiveOps::eChallengeState>(result.getIntField("state"));
		row.milestone = static_cast<uint8_t>(U32(result, "milestone"));
		row.completedAt = result.getInt64Field("completed_at");
		row.rewardedAt = result.getInt64Field("rewarded_at");
		row.rewardedCount = U32(result, "rewarded_count");
		row.endAnnouncedAt = result.getInt64Field("end_announced_at");
		row.createdAt = result.getInt64Field("created_at");
		row.createdBy = Text(result, "created_by");
		row.updatedAt = result.getInt64Field("updated_at");
		row.updatedBy = Text(result, "updated_by");
		return row;
	}

	ILiveOps::Reward ReadReward(CppSQLite3Query& result) {
		return { U64(result, "challenge_id"), result.getInt64Field("character_id"), result.getInt64Field("amount"), result.getInt64Field("coins"),
			result.getInt64Field("rewarded_at"), result.getInt64Field("claimed_at") };
	}

	std::vector<ILiveOps::LiveOpsScore> ReadScores(CppSQLite3Query& result, const char* idField) {
		std::vector<ILiveOps::LiveOpsScore> scores;
		for (; !result.eof(); result.nextRow()) {
			scores.push_back({ U64(result, idField), result.getInt64Field("character_id"), Text(result, "name"), result.getInt64Field("amount") });
		}
		return scores;
	}
}

std::vector<ILiveOps::LiveEvent> SQLiteDatabase::GetLiveEvents(bool activeOnly, uint32_t limit) {
	std::vector<LiveEvent> rows;
	auto [_, result] = ExecuteSelect(std::string("SELECT * FROM live_events") + (activeOnly ? " WHERE state = 0" : "") + " ORDER BY id DESC LIMIT ?;", limit);
	for (; !result.eof(); result.nextRow()) rows.push_back(ReadEvent(result));
	return rows;
}

std::optional<ILiveOps::LiveEvent> SQLiteDatabase::GetLiveEvent(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM live_events WHERE id = ?;", id);
	if (result.eof()) return std::nullopt;
	return ReadEvent(result);
}

uint64_t SQLiteDatabase::InsertLiveEvent(const LiveEvent& row) {
	ExecuteInsert("INSERT INTO live_events (type, title, message, zones, instance_id, config, starts_at, ends_at, state, created_by) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		row.type, row.title, row.message, JoinZones(row.zones), row.instanceId, row.config, row.startsAt, row.endsAt, static_cast<uint8_t>(row.state), row.createdBy);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : U64(result, "id");
}

bool SQLiteDatabase::EndLiveEvent(uint64_t id, eLiveEventState state, int64_t endedAt, const std::string& endedBy, const std::string& reason) {
	return ExecuteUpdate("UPDATE live_events SET state = ?, ended_at = ?, ended_by = ?, end_reason = ? WHERE id = ? AND state = 0;",
		static_cast<uint8_t>(state), endedAt, endedBy, reason, id) > 0;
}

std::vector<ILiveOps::LiveEventInstance> SQLiteDatabase::GetLiveEventInstances(const std::vector<uint64_t>& eventIds) {
	std::vector<LiveEventInstance> rows;
	for (const auto eventId : eventIds) {
		auto [_, result] = ExecuteSelect("SELECT * FROM live_event_instances WHERE event_id = ? ORDER BY zone_id, instance_id;", eventId);
		for (; !result.eof(); result.nextRow()) {
			rows.push_back({ U64(result, "event_id"), U32(result, "zone_id"), U32(result, "instance_id"), Text(result, "status"), result.getInt64Field("updated_at") });
		}
	}
	return rows;
}

void SQLiteDatabase::SetLiveEventInstance(const LiveEventInstance& row) {
	ExecuteInsert("INSERT INTO live_event_instances (event_id, zone_id, instance_id, status, updated_at) VALUES (?, ?, ?, ?, ?) "
		"ON CONFLICT(event_id, zone_id, instance_id) DO UPDATE SET status = excluded.status, updated_at = excluded.updated_at;",
		row.eventId, row.zoneId, row.instanceId, row.status, row.updatedAt);
}

void SQLiteDatabase::AddLiveEventScores(uint64_t eventId, const std::vector<std::pair<LWOOBJID, int64_t>>& scores, int64_t time) {
	for (const auto& [characterId, amount] : scores) {
		ExecuteInsert("INSERT INTO live_event_scores (event_id, character_id, amount, updated_at) VALUES (?, ?, ?, ?) "
			"ON CONFLICT(event_id, character_id) DO UPDATE SET amount = amount + excluded.amount, updated_at = excluded.updated_at;",
			eventId, characterId, amount, time);
	}
}

std::vector<ILiveOps::LiveOpsScore> SQLiteDatabase::GetLiveEventScores(uint64_t eventId, uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT s.event_id, s.character_id, c.name, s.amount FROM live_event_scores s LEFT JOIN charinfo c ON c.id = s.character_id "
		"WHERE s.event_id = ? ORDER BY s.amount DESC, s.updated_at LIMIT ?;", eventId, limit);
	return ReadScores(result, "event_id");
}

std::vector<ILiveOps::Challenge> SQLiteDatabase::GetChallenges(bool openOnly) {
	std::vector<Challenge> rows;
	auto [_, result] = ExecuteSelect(std::string("SELECT * FROM challenges") + (openOnly ? " WHERE state = 0" : "") + " ORDER BY id DESC;");
	for (; !result.eof(); result.nextRow()) rows.push_back(ReadChallenge(result));
	return rows;
}

std::optional<ILiveOps::Challenge> SQLiteDatabase::GetChallenge(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM challenges WHERE id = ?;", id);
	if (result.eof()) return std::nullopt;
	return ReadChallenge(result);
}

uint64_t SQLiteDatabase::InsertChallenge(const Challenge& row) {
	ExecuteInsert("INSERT INTO challenges (title, description, metric_kind, metric, lot, zones, target, starts_at, ends_at, include_staff, is_public, reward_coins, "
		"reward_items, reward_min, state, milestone, created_at, created_by, updated_at, updated_by) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		row.title, row.description, static_cast<uint8_t>(row.metricKind), row.metric, row.lot, JoinZones(row.zones), row.target, row.startsAt, row.endsAt,
		row.includeStaff, row.isPublic, row.rewardCoins, row.rewardItems, row.rewardMin, static_cast<uint8_t>(row.state), row.milestone,
		row.createdAt, row.createdBy, row.updatedAt, row.updatedBy);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : U64(result, "id");
}

void SQLiteDatabase::UpdateChallenge(const Challenge& row) {
	ExecuteUpdate("UPDATE challenges SET title = ?, description = ?, metric_kind = ?, metric = ?, lot = ?, zones = ?, target = ?, starts_at = ?, ends_at = ?, "
		"include_staff = ?, is_public = ?, reward_coins = ?, reward_items = ?, reward_min = ?, state = ?, milestone = ?, completed_at = ?, rewarded_at = ?, "
		"rewarded_count = ?, end_announced_at = ?, updated_at = ?, updated_by = ? WHERE id = ?;",
		row.title, row.description, static_cast<uint8_t>(row.metricKind), row.metric, row.lot, JoinZones(row.zones), row.target, row.startsAt, row.endsAt,
		row.includeStaff, row.isPublic, row.rewardCoins, row.rewardItems, row.rewardMin, static_cast<uint8_t>(row.state), row.milestone, row.completedAt,
		row.rewardedAt, row.rewardedCount, row.endAnnouncedAt, row.updatedAt, row.updatedBy, row.id);
}

void SQLiteDatabase::DeleteChallenge(uint64_t id) {
	ExecuteDelete("DELETE FROM challenge_contributions WHERE challenge_id = ?;", id);
	ExecuteDelete("DELETE FROM challenge_rewards WHERE challenge_id = ?;", id);
	ExecuteDelete("DELETE FROM challenges WHERE id = ?;", id);
}

void SQLiteDatabase::AddChallengeContributions(const std::vector<Contribution>& contributions, int64_t time) {
	for (const auto& row : contributions) {
		ExecuteInsert("INSERT INTO challenge_contributions (challenge_id, character_id, amount, updated_at) VALUES (?, ?, ?, ?) "
			"ON CONFLICT(challenge_id, character_id) DO UPDATE SET amount = amount + excluded.amount, updated_at = excluded.updated_at;",
			row.challengeId, row.characterId, row.amount, time);
	}
}

std::map<uint64_t, ILiveOps::ChallengeTotal> SQLiteDatabase::GetChallengeTotals(const std::vector<uint64_t>& challengeIds) {
	std::map<uint64_t, ChallengeTotal> totals;
	for (const auto id : challengeIds) {
		auto [_, result] = ExecuteSelect("SELECT COALESCE(SUM(amount), 0) AS total, COUNT(*) AS contributors FROM challenge_contributions WHERE challenge_id = ? AND amount > 0;", id);
		if (!result.eof()) totals[id] = { result.getInt64Field("total"), U32(result, "contributors") };
	}
	return totals;
}

std::vector<ILiveOps::LiveOpsScore> SQLiteDatabase::GetChallengeContributions(uint64_t challengeId, uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT s.challenge_id, s.character_id, c.name, s.amount FROM challenge_contributions s LEFT JOIN charinfo c ON c.id = s.character_id "
		"WHERE s.challenge_id = ? AND s.amount > 0 ORDER BY s.amount DESC, s.updated_at LIMIT ?;", challengeId, limit == 0 ? int64_t{ -1 } : static_cast<int64_t>(limit));
	return ReadScores(result, "challenge_id");
}

std::map<uint64_t, int64_t> SQLiteDatabase::GetCharacterContributions(LWOOBJID characterId) {
	std::map<uint64_t, int64_t> amounts;
	auto [_, result] = ExecuteSelect("SELECT challenge_id, amount FROM challenge_contributions WHERE character_id = ?;", characterId);
	for (; !result.eof(); result.nextRow()) amounts[U64(result, "challenge_id")] = result.getInt64Field("amount");
	return amounts;
}

std::vector<ILiveOps::Reward> SQLiteDatabase::GetChallengeRewards(uint64_t challengeId) {
	std::vector<Reward> rows;
	auto [_, result] = ExecuteSelect("SELECT * FROM challenge_rewards WHERE challenge_id = ?;", challengeId);
	for (; !result.eof(); result.nextRow()) rows.push_back(ReadReward(result));
	return rows;
}

void SQLiteDatabase::InsertChallengeReward(const Reward& row) {
	ExecuteInsert("INSERT OR IGNORE INTO challenge_rewards (challenge_id, character_id, amount, coins, rewarded_at, claimed_at) VALUES (?, ?, ?, ?, ?, ?);",
		row.challengeId, row.characterId, row.amount, row.coins, row.rewardedAt, row.claimedAt);
}

std::vector<ILiveOps::Reward> SQLiteDatabase::GetUnclaimedChallengeCoins(LWOOBJID characterId) {
	std::vector<Reward> rows;
	auto [_, result] = ExecuteSelect("SELECT * FROM challenge_rewards WHERE character_id = ? AND claimed_at = 0 AND coins > 0;", characterId);
	for (; !result.eof(); result.nextRow()) rows.push_back(ReadReward(result));
	return rows;
}

bool SQLiteDatabase::ClaimChallengeCoins(uint64_t challengeId, LWOOBJID characterId, int64_t time) {
	return ExecuteUpdate("UPDATE challenge_rewards SET claimed_at = ? WHERE challenge_id = ? AND character_id = ? AND claimed_at = 0;", time, challengeId, characterId) > 0;
}
