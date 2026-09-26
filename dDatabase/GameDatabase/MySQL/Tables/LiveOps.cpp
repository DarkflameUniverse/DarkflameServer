#include "MySQLDatabase.h"

#include "GeneralUtils.h"

namespace {
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

	std::string Text(PreparedStmtResultSet& result, const char* field) {
		return result->isNull(field) ? "" : std::string(result->getString(field).c_str());
	}

	ILiveOps::LiveEvent ReadEvent(PreparedStmtResultSet& result) {
		ILiveOps::LiveEvent row;
		row.id = result->getUInt64("id");
		row.type = Text(result, "type");
		row.title = Text(result, "title");
		row.message = Text(result, "message");
		row.zones = SplitZones(Text(result, "zones"));
		row.instanceId = result->getInt("instance_id");
		row.config = Text(result, "config");
		row.startsAt = result->getInt64("starts_at");
		row.endsAt = result->getInt64("ends_at");
		row.state = static_cast<ILiveOps::eLiveEventState>(result->getInt("state"));
		row.endedAt = result->getInt64("ended_at");
		row.endReason = Text(result, "end_reason");
		row.createdBy = Text(result, "created_by");
		row.endedBy = Text(result, "ended_by");
		return row;
	}

	ILiveOps::Challenge ReadChallenge(PreparedStmtResultSet& result) {
		ILiveOps::Challenge row;
		row.id = result->getUInt64("id");
		row.title = Text(result, "title");
		row.description = Text(result, "description");
		row.metricKind = static_cast<ILiveOps::eChallengeMetric>(result->getInt("metric_kind"));
		row.metric = result->getUInt("metric");
		row.lot = result->getInt("lot");
		row.zones = SplitZones(Text(result, "zones"));
		row.target = result->getInt64("target");
		row.startsAt = result->getInt64("starts_at");
		row.endsAt = result->getInt64("ends_at");
		row.includeStaff = result->getInt("include_staff") != 0;
		row.isPublic = result->getInt("is_public") != 0;
		row.rewardCoins = result->getInt64("reward_coins");
		row.rewardItems = Text(result, "reward_items");
		row.rewardMin = result->getInt64("reward_min");
		row.state = static_cast<ILiveOps::eChallengeState>(result->getInt("state"));
		row.milestone = static_cast<uint8_t>(result->getUInt("milestone"));
		row.completedAt = result->getInt64("completed_at");
		row.rewardedAt = result->getInt64("rewarded_at");
		row.rewardedCount = result->getUInt("rewarded_count");
		row.endAnnouncedAt = result->getInt64("end_announced_at");
		row.createdAt = result->getInt64("created_at");
		row.createdBy = Text(result, "created_by");
		row.updatedAt = result->getInt64("updated_at");
		row.updatedBy = Text(result, "updated_by");
		return row;
	}

	ILiveOps::Reward ReadReward(PreparedStmtResultSet& result) {
		return { result->getUInt64("challenge_id"), result->getInt64("character_id"), result->getInt64("amount"), result->getInt64("coins"),
			result->getInt64("rewarded_at"), result->getInt64("claimed_at") };
	}

	std::vector<ILiveOps::LiveOpsScore> ReadScores(PreparedStmtResultSet& result, const char* idField) {
		std::vector<ILiveOps::LiveOpsScore> scores;
		while (result->next()) {
			scores.push_back({ result->getUInt64(idField), result->getInt64("character_id"), Text(result, "name"), result->getInt64("amount") });
		}
		return scores;
	}
}

std::vector<ILiveOps::LiveEvent> MySQLDatabase::GetLiveEvents(bool activeOnly, uint32_t limit) {
	std::vector<LiveEvent> rows;
	auto result = ExecuteSelect(std::string("SELECT * FROM live_events") + (activeOnly ? " WHERE state = 0" : "") + " ORDER BY id DESC LIMIT ?;", limit);
	while (result->next()) rows.push_back(ReadEvent(result));
	return rows;
}

std::optional<ILiveOps::LiveEvent> MySQLDatabase::GetLiveEvent(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM live_events WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	return ReadEvent(result);
}

uint64_t MySQLDatabase::InsertLiveEvent(const LiveEvent& row) {
	ExecuteInsert("INSERT INTO live_events (type, title, message, zones, instance_id, config, starts_at, ends_at, state, created_by) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		row.type, row.title, row.message, JoinZones(row.zones), row.instanceId, row.config, row.startsAt, row.endsAt, static_cast<uint8_t>(row.state), row.createdBy);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return result->next() ? result->getUInt64("id") : 0;
}

bool MySQLDatabase::EndLiveEvent(uint64_t id, eLiveEventState state, int64_t endedAt, const std::string& endedBy, const std::string& reason) {
	return ExecuteUpdate("UPDATE live_events SET state = ?, ended_at = ?, ended_by = ?, end_reason = ? WHERE id = ? AND state = 0;",
		static_cast<uint8_t>(state), endedAt, endedBy, reason, id) > 0;
}

std::vector<ILiveOps::LiveEventInstance> MySQLDatabase::GetLiveEventInstances(const std::vector<uint64_t>& eventIds) {
	std::vector<LiveEventInstance> rows;
	for (const auto eventId : eventIds) {
		auto result = ExecuteSelect("SELECT * FROM live_event_instances WHERE event_id = ? ORDER BY zone_id, instance_id;", eventId);
		while (result->next()) {
			rows.push_back({ result->getUInt64("event_id"), result->getUInt("zone_id"), result->getUInt("instance_id"), Text(result, "status"), result->getInt64("updated_at") });
		}
	}
	return rows;
}

void MySQLDatabase::SetLiveEventInstance(const LiveEventInstance& row) {
	ExecuteInsert("INSERT INTO live_event_instances (event_id, zone_id, instance_id, status, updated_at) VALUES (?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE status = VALUES(status), updated_at = VALUES(updated_at);",
		row.eventId, row.zoneId, row.instanceId, row.status, row.updatedAt);
}

void MySQLDatabase::AddLiveEventScores(uint64_t eventId, const std::vector<std::pair<LWOOBJID, int64_t>>& scores, int64_t time) {
	for (const auto& [characterId, amount] : scores) {
		ExecuteInsert("INSERT INTO live_event_scores (event_id, character_id, amount, updated_at) VALUES (?, ?, ?, ?) "
			"ON DUPLICATE KEY UPDATE amount = amount + VALUES(amount), updated_at = VALUES(updated_at);",
			eventId, characterId, amount, time);
	}
}

std::vector<ILiveOps::LiveOpsScore> MySQLDatabase::GetLiveEventScores(uint64_t eventId, uint32_t limit) {
	auto result = ExecuteSelect("SELECT s.event_id, s.character_id, c.name, s.amount FROM live_event_scores s LEFT JOIN charinfo c ON c.id = s.character_id "
		"WHERE s.event_id = ? ORDER BY s.amount DESC, s.updated_at LIMIT ?;", eventId, limit);
	return ReadScores(result, "event_id");
}

std::vector<ILiveOps::Challenge> MySQLDatabase::GetChallenges(bool openOnly) {
	std::vector<Challenge> rows;
	auto result = ExecuteSelect(std::string("SELECT * FROM challenges") + (openOnly ? " WHERE state = 0" : "") + " ORDER BY id DESC;");
	while (result->next()) rows.push_back(ReadChallenge(result));
	return rows;
}

std::optional<ILiveOps::Challenge> MySQLDatabase::GetChallenge(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM challenges WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	return ReadChallenge(result);
}

uint64_t MySQLDatabase::InsertChallenge(const Challenge& row) {
	ExecuteInsert("INSERT INTO challenges (title, description, metric_kind, metric, lot, zones, target, starts_at, ends_at, include_staff, is_public, reward_coins, "
		"reward_items, reward_min, state, milestone, created_at, created_by, updated_at, updated_by) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		row.title, row.description, static_cast<uint8_t>(row.metricKind), row.metric, row.lot, JoinZones(row.zones), row.target, row.startsAt, row.endsAt,
		row.includeStaff, row.isPublic, row.rewardCoins, row.rewardItems, row.rewardMin, static_cast<uint8_t>(row.state), row.milestone,
		row.createdAt, row.createdBy, row.updatedAt, row.updatedBy);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return result->next() ? result->getUInt64("id") : 0;
}

void MySQLDatabase::UpdateChallenge(const Challenge& row) {
	ExecuteUpdate("UPDATE challenges SET title = ?, description = ?, metric_kind = ?, metric = ?, lot = ?, zones = ?, target = ?, starts_at = ?, ends_at = ?, "
		"include_staff = ?, is_public = ?, reward_coins = ?, reward_items = ?, reward_min = ?, state = ?, milestone = ?, completed_at = ?, rewarded_at = ?, "
		"rewarded_count = ?, end_announced_at = ?, updated_at = ?, updated_by = ? WHERE id = ?;",
		row.title, row.description, static_cast<uint8_t>(row.metricKind), row.metric, row.lot, JoinZones(row.zones), row.target, row.startsAt, row.endsAt,
		row.includeStaff, row.isPublic, row.rewardCoins, row.rewardItems, row.rewardMin, static_cast<uint8_t>(row.state), row.milestone, row.completedAt,
		row.rewardedAt, row.rewardedCount, row.endAnnouncedAt, row.updatedAt, row.updatedBy, row.id);
}

void MySQLDatabase::DeleteChallenge(uint64_t id) {
	ExecuteDelete("DELETE FROM challenge_contributions WHERE challenge_id = ?;", id);
	ExecuteDelete("DELETE FROM challenge_rewards WHERE challenge_id = ?;", id);
	ExecuteDelete("DELETE FROM challenges WHERE id = ?;", id);
}

void MySQLDatabase::AddChallengeContributions(const std::vector<Contribution>& contributions, int64_t time) {
	for (const auto& row : contributions) {
		ExecuteInsert("INSERT INTO challenge_contributions (challenge_id, character_id, amount, updated_at) VALUES (?, ?, ?, ?) "
			"ON DUPLICATE KEY UPDATE amount = amount + VALUES(amount), updated_at = VALUES(updated_at);",
			row.challengeId, row.characterId, row.amount, time);
	}
}

std::map<uint64_t, ILiveOps::ChallengeTotal> MySQLDatabase::GetChallengeTotals(const std::vector<uint64_t>& challengeIds) {
	std::map<uint64_t, ChallengeTotal> totals;
	for (const auto id : challengeIds) {
		auto result = ExecuteSelect("SELECT COALESCE(SUM(amount), 0) AS total, COUNT(*) AS contributors FROM challenge_contributions WHERE challenge_id = ? AND amount > 0;", id);
		if (result->next()) totals[id] = { result->getInt64("total"), result->getUInt("contributors") };
	}
	return totals;
}

std::vector<ILiveOps::LiveOpsScore> MySQLDatabase::GetChallengeContributions(uint64_t challengeId, uint32_t limit) {
	auto result = ExecuteSelect("SELECT s.challenge_id, s.character_id, c.name, s.amount FROM challenge_contributions s LEFT JOIN charinfo c ON c.id = s.character_id "
		"WHERE s.challenge_id = ? AND s.amount > 0 ORDER BY s.amount DESC, s.updated_at LIMIT ?;", challengeId, limit == 0 ? UINT32_MAX : limit);
	return ReadScores(result, "challenge_id");
}

std::map<uint64_t, int64_t> MySQLDatabase::GetCharacterContributions(LWOOBJID characterId) {
	std::map<uint64_t, int64_t> amounts;
	auto result = ExecuteSelect("SELECT challenge_id, amount FROM challenge_contributions WHERE character_id = ?;", characterId);
	while (result->next()) amounts[result->getUInt64("challenge_id")] = result->getInt64("amount");
	return amounts;
}

std::vector<ILiveOps::Reward> MySQLDatabase::GetChallengeRewards(uint64_t challengeId) {
	std::vector<Reward> rows;
	auto result = ExecuteSelect("SELECT * FROM challenge_rewards WHERE challenge_id = ?;", challengeId);
	while (result->next()) rows.push_back(ReadReward(result));
	return rows;
}

void MySQLDatabase::InsertChallengeReward(const Reward& row) {
	ExecuteInsert("INSERT IGNORE INTO challenge_rewards (challenge_id, character_id, amount, coins, rewarded_at, claimed_at) VALUES (?, ?, ?, ?, ?, ?);",
		row.challengeId, row.characterId, row.amount, row.coins, row.rewardedAt, row.claimedAt);
}

std::vector<ILiveOps::Reward> MySQLDatabase::GetUnclaimedChallengeCoins(LWOOBJID characterId) {
	std::vector<Reward> rows;
	auto result = ExecuteSelect("SELECT * FROM challenge_rewards WHERE character_id = ? AND claimed_at = 0 AND coins > 0;", characterId);
	while (result->next()) rows.push_back(ReadReward(result));
	return rows;
}

bool MySQLDatabase::ClaimChallengeCoins(uint64_t challengeId, LWOOBJID characterId, int64_t time) {
	return ExecuteUpdate("UPDATE challenge_rewards SET claimed_at = ? WHERE challenge_id = ? AND character_id = ? AND claimed_at = 0;", time, challengeId, characterId) > 0;
}
