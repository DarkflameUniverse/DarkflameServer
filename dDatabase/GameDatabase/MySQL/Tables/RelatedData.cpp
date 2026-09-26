#include "MySQLDatabase.h"

nlohmann::json MySQLDatabase::GetPropertiesOwnedBy(LWOOBJID characterId) {
	nlohmann::json rows = nlohmann::json::array();
	auto result = ExecuteSelect("SELECT p.id, p.name, p.zone_id, p.privacy_option, p.mod_approved, p.rejection_reason, p.last_updated, p.reputation, (SELECT COUNT(*) FROM properties_contents c WHERE c.property_id = p.id) AS models FROM properties p WHERE p.owner_id = ? ORDER BY p.last_updated DESC;", characterId);
	while (result->next()) {
		rows.push_back({ {"id", std::to_string(result->getInt64("id"))}, {"name", std::string(result->isNull("name") ? "" : result->getString("name").c_str())}, {"zone_id", result->getInt("zone_id")}, {"privacy_option", result->getInt("privacy_option")},
			{"mod_approved", result->getInt("mod_approved") != 0}, {"rejection_reason", std::string(result->isNull("rejection_reason") ? "" : result->getString("rejection_reason").c_str())}, {"last_updated", result->getInt64("last_updated")}, {"reputation", result->getInt64("reputation")}, {"models", result->getInt("models")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetBugReportsBy(LWOOBJID characterId, uint32_t limit) {
	nlohmann::json rows = nlohmann::json::array();
	auto result = ExecuteSelect("SELECT id, body, submitted, resolved_time FROM bug_reports WHERE reporter_id = ? ORDER BY id DESC LIMIT ?;", characterId, limit);
	while (result->next()) {
		rows.push_back({ {"id", result->getInt("id")}, {"body", std::string(result->isNull("body") ? "" : result->getString("body").c_str()).substr(0, 200)}, {"submitted", std::string(result->isNull("submitted") ? "" : result->getString("submitted").c_str())}, {"resolved", !result->isNull("resolved_time")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetEconomyFlagsFor(LWOOBJID characterId, uint32_t limit) {
	nlohmann::json rows = nlohmann::json::array();
	auto result = ExecuteSelect("SELECT id, created_at, kind, details, status FROM economy_flags WHERE character_id = ? ORDER BY id DESC LIMIT ?;", characterId, limit);
	while (result->next()) {
		rows.push_back({ {"id", result->getInt64("id")}, {"created_at", result->getInt64("created_at")}, {"kind", result->getInt("kind")}, {"details", std::string(result->isNull("details") ? "" : result->getString("details").c_str())}, {"status", result->getInt("status")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetFriendsOf(LWOOBJID characterId) {
	nlohmann::json rows = nlohmann::json::array();
	auto result = ExecuteSelect("SELECT f.friend_id AS id, f.best_friend AS best, c.name FROM friends f LEFT JOIN charinfo c ON c.id = f.friend_id WHERE f.player_id = ? UNION SELECT f.player_id AS id, f.best_friend AS best, c.name FROM friends f LEFT JOIN charinfo c ON c.id = f.player_id WHERE f.friend_id = ?;", characterId, characterId);
	while (result->next()) {
		rows.push_back({ {"id", std::to_string(result->getInt64("id"))}, {"name", std::string(result->isNull("name") ? "" : result->getString("name").c_str())}, {"best", result->getInt("best") != 0} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetCheatDetectionsFor(uint32_t accountId, uint32_t limit) {
	nlohmann::json rows = nlohmann::json::array();
	auto result = ExecuteSelect("SELECT id, name, violation_msg, violation_time FROM player_cheat_detections WHERE account_id = ? ORDER BY id DESC LIMIT ?;", accountId, limit);
	while (result->next()) {
		rows.push_back({ {"id", result->getInt64("id")}, {"name", std::string(result->isNull("name") ? "" : result->getString("name").c_str())}, {"message", std::string(result->isNull("violation_msg") ? "" : result->getString("violation_msg").c_str())}, {"time", std::string(result->isNull("violation_time") ? "" : result->getString("violation_time").c_str())} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetAuditAbout(uint32_t accountId, uint32_t limit) {
	nlohmann::json rows = nlohmann::json::array();
	auto result = ExecuteSelect("SELECT id, account_name, action, description, timestamp FROM audit_log WHERE target_account_id = ? ORDER BY id DESC LIMIT ?;", accountId, limit);
	while (result->next()) {
		rows.push_back({ {"id", result->getInt64("id")}, {"actor", std::string(result->isNull("account_name") ? "" : result->getString("account_name").c_str())}, {"action", std::string(result->isNull("action") ? "" : result->getString("action").c_str())}, {"description", std::string(result->isNull("description") ? "" : result->getString("description").c_str())}, {"time", result->getInt64("timestamp")} });
	}
	return rows;
}

void MySQLDatabase::InsertModerationDecision(const std::string& kind, int64_t subjectId, const std::string& subject, bool approved, const std::string& reason, int64_t time) {
	ExecuteInsert("INSERT INTO moderation_decisions (kind, subject_id, subject, approved, reason, decided_at) VALUES (?, ?, ?, ?, ?, ?);", kind, subjectId, subject, approved, reason, time);
}

nlohmann::json MySQLDatabase::GetModerationDecisions(const std::string& kind, int64_t subjectId, uint32_t limit) {
	nlohmann::json rows = nlohmann::json::array();
	auto result = ExecuteSelect("SELECT subject, approved, reason, decided_at FROM moderation_decisions WHERE kind = ? AND subject_id = ? ORDER BY id DESC LIMIT ?;", kind, subjectId, limit);
	while (result->next()) {
		rows.push_back({ {"subject", result->getString("subject").c_str()}, {"approved", result->getInt("approved") != 0}, {"reason", result->getString("reason").c_str()}, {"time", result->getInt64("decided_at")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetPropertyRecord(LWOOBJID propertyId) {
	auto result = ExecuteSelect("SELECT id, owner_id, template_id, clone_id, name, description, rent_amount, rent_due, privacy_option, mod_approved, last_updated, time_claimed, rejection_reason, reputation, zone_id, performance_cost FROM properties WHERE id = ? LIMIT 1;", propertyId);
	if (!result->next()) return nlohmann::json::object();
	const auto text = [&result](const char* col) { return std::string(result->isNull(col) ? "" : result->getString(col).c_str()); };
	const auto id = [&result](const char* col) -> nlohmann::json { return result->isNull(col) ? nlohmann::json() : nlohmann::json(std::to_string(result->getInt64(col))); };
	return {
		{"id", id("id")}, {"owner_id", id("owner_id")}, {"template_id", result->getInt("template_id")}, {"clone_id", id("clone_id")},
		{"name", text("name")}, {"description", text("description")}, {"rent_amount", result->getInt("rent_amount")}, {"rent_due", result->getInt64("rent_due")},
		{"privacy_option", result->getInt("privacy_option")}, {"mod_approved", result->getInt("mod_approved")}, {"last_updated", result->getInt64("last_updated")},
		{"time_claimed", result->getInt64("time_claimed")}, {"rejection_reason", text("rejection_reason")}, {"reputation", result->getInt64("reputation")},
		{"zone_id", result->getInt("zone_id")}, {"performance_cost", result->isNull("performance_cost") ? 0.0 : static_cast<double>(result->getDouble("performance_cost"))}
	};
}

nlohmann::json MySQLDatabase::GetPropertyModelRecords(LWOOBJID propertyId) {
	nlohmann::json rows = nlohmann::json::array();
	auto result = ExecuteSelect("SELECT pc.id, pc.property_id, pc.ugc_id, pc.lot, pc.x, pc.y, pc.z, pc.rx, pc.ry, pc.rz, pc.rw, pc.model_name, pc.model_description, "
		"pc.behavior_1, pc.behavior_2, pc.behavior_3, pc.behavior_4, pc.behavior_5, u.account_id, u.character_id AS ugc_character_id, u.is_optimized, u.bake_ao, u.filename, "
		"LENGTH(u.lxfml) AS lxfml_size, mb.ldf_config, c.name AS creator_name FROM properties_contents pc LEFT JOIN ugc u ON u.id = pc.ugc_id "
		"LEFT JOIN ugc_modular_build mb ON mb.ugc_id = pc.ugc_id LEFT JOIN charinfo c ON c.id = u.character_id WHERE pc.property_id = ? ORDER BY pc.id;", propertyId);
	while (result->next()) {
		const auto text = [&result](const char* col) -> nlohmann::json { return result->isNull(col) ? nlohmann::json() : nlohmann::json(std::string(result->getString(col).c_str())); };
		const auto id = [&result](const char* col) -> nlohmann::json { return result->isNull(col) ? nlohmann::json() : nlohmann::json(std::to_string(result->getInt64(col))); };
		const auto num = [&result](const char* col) -> nlohmann::json { return result->isNull(col) ? nlohmann::json() : nlohmann::json(static_cast<int64_t>(result->getInt64(col))); };
		const auto dbl = [&result](const char* col) { return static_cast<double>(result->getDouble(col)); };
		rows.push_back({
			{"id", id("id")}, {"property_id", id("property_id")}, {"ugc_id", id("ugc_id")}, {"lot", result->getInt("lot")},
			{"x", dbl("x")}, {"y", dbl("y")}, {"z", dbl("z")}, {"rx", dbl("rx")}, {"ry", dbl("ry")}, {"rz", dbl("rz")}, {"rw", dbl("rw")},
			{"model_name", text("model_name")}, {"model_description", text("model_description")},
			{"behavior_1", id("behavior_1")}, {"behavior_2", id("behavior_2")}, {"behavior_3", id("behavior_3")}, {"behavior_4", id("behavior_4")}, {"behavior_5", id("behavior_5")},
			{"account_id", num("account_id")}, {"ugc_character_id", id("ugc_character_id")}, {"is_optimized", num("is_optimized")}, {"bake_ao", num("bake_ao")},
			{"filename", text("filename")}, {"lxfml_size", num("lxfml_size")}, {"ldf_config", text("ldf_config")}, {"creator_name", text("creator_name")}
		});
	}
	return rows;
}
