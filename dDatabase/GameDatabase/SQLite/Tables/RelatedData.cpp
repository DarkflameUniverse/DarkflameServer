#include "SQLiteDatabase.h"

nlohmann::json SQLiteDatabase::GetPropertiesOwnedBy(LWOOBJID characterId) {
	nlohmann::json rows = nlohmann::json::array();
	auto [_, result] = ExecuteSelect("SELECT p.id, p.name, p.zone_id, p.privacy_option, p.mod_approved, p.rejection_reason, p.last_updated, p.reputation, (SELECT COUNT(*) FROM properties_contents c WHERE c.property_id = p.id) AS models FROM properties p WHERE p.owner_id = ? ORDER BY p.last_updated DESC;", characterId);
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"id", std::to_string(result.getInt64Field("id"))}, {"name", std::string(result.fieldIsNull("name") ? "" : result.getStringField("name"))}, {"zone_id", result.getIntField("zone_id")}, {"privacy_option", result.getIntField("privacy_option")},
			{"mod_approved", result.getIntField("mod_approved") != 0}, {"rejection_reason", std::string(result.fieldIsNull("rejection_reason") ? "" : result.getStringField("rejection_reason"))}, {"last_updated", result.getInt64Field("last_updated")}, {"reputation", result.getInt64Field("reputation")}, {"models", result.getIntField("models")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetBugReportsBy(LWOOBJID characterId, uint32_t limit) {
	nlohmann::json rows = nlohmann::json::array();
	auto [_, result] = ExecuteSelect("SELECT id, body, submitted, resolved_time FROM bug_reports WHERE reporter_id = ? ORDER BY id DESC LIMIT ?;", characterId, limit);
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"id", result.getIntField("id")}, {"body", std::string(result.fieldIsNull("body") ? "" : result.getStringField("body")).substr(0, 200)}, {"submitted", std::string(result.fieldIsNull("submitted") ? "" : result.getStringField("submitted"))}, {"resolved", !result.fieldIsNull("resolved_time")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetEconomyFlagsFor(LWOOBJID characterId, uint32_t limit) {
	nlohmann::json rows = nlohmann::json::array();
	auto [_, result] = ExecuteSelect("SELECT id, created_at, kind, details, status FROM economy_flags WHERE character_id = ? ORDER BY id DESC LIMIT ?;", characterId, limit);
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"id", result.getInt64Field("id")}, {"created_at", result.getInt64Field("created_at")}, {"kind", result.getIntField("kind")}, {"details", std::string(result.fieldIsNull("details") ? "" : result.getStringField("details"))}, {"status", result.getIntField("status")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetFriendsOf(LWOOBJID characterId) {
	nlohmann::json rows = nlohmann::json::array();
	auto [_, result] = ExecuteSelect("SELECT f.friend_id AS id, f.best_friend AS best, c.name FROM friends f LEFT JOIN charinfo c ON c.id = f.friend_id WHERE f.player_id = ? UNION SELECT f.player_id AS id, f.best_friend AS best, c.name FROM friends f LEFT JOIN charinfo c ON c.id = f.player_id WHERE f.friend_id = ?;", characterId, characterId);
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"id", std::to_string(result.getInt64Field("id"))}, {"name", std::string(result.fieldIsNull("name") ? "" : result.getStringField("name"))}, {"best", result.getIntField("best") != 0} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetCheatDetectionsFor(uint32_t accountId, uint32_t limit) {
	nlohmann::json rows = nlohmann::json::array();
	auto [_, result] = ExecuteSelect("SELECT id, name, violation_msg, violation_time FROM player_cheat_detections WHERE account_id = ? ORDER BY id DESC LIMIT ?;", accountId, limit);
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"id", result.getInt64Field("id")}, {"name", std::string(result.fieldIsNull("name") ? "" : result.getStringField("name"))}, {"message", std::string(result.fieldIsNull("violation_msg") ? "" : result.getStringField("violation_msg"))}, {"time", std::string(result.fieldIsNull("violation_time") ? "" : result.getStringField("violation_time"))} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetAuditAbout(uint32_t accountId, uint32_t limit) {
	nlohmann::json rows = nlohmann::json::array();
	auto [_, result] = ExecuteSelect("SELECT id, account_name, action, description, timestamp FROM audit_log WHERE target_account_id = ? ORDER BY id DESC LIMIT ?;", accountId, limit);
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"id", result.getInt64Field("id")}, {"actor", std::string(result.fieldIsNull("account_name") ? "" : result.getStringField("account_name"))}, {"action", std::string(result.fieldIsNull("action") ? "" : result.getStringField("action"))}, {"description", std::string(result.fieldIsNull("description") ? "" : result.getStringField("description"))}, {"time", result.getInt64Field("timestamp")} });
	}
	return rows;
}

void SQLiteDatabase::InsertModerationDecision(const std::string& kind, int64_t subjectId, const std::string& subject, bool approved, const std::string& reason, int64_t time) {
	ExecuteInsert("INSERT INTO moderation_decisions (kind, subject_id, subject, approved, reason, decided_at) VALUES (?, ?, ?, ?, ?, ?);", kind, subjectId, subject, approved, reason, time);
}

nlohmann::json SQLiteDatabase::GetModerationDecisions(const std::string& kind, int64_t subjectId, uint32_t limit) {
	nlohmann::json rows = nlohmann::json::array();
	auto [_, result] = ExecuteSelect("SELECT subject, approved, reason, decided_at FROM moderation_decisions WHERE kind = ? AND subject_id = ? ORDER BY id DESC LIMIT ?;", kind, subjectId, limit);
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"subject", result.getStringField("subject")}, {"approved", result.getIntField("approved") != 0}, {"reason", result.getStringField("reason")}, {"time", result.getInt64Field("decided_at")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetPropertyRecord(LWOOBJID propertyId) {
	auto [_, result] = ExecuteSelect("SELECT id, owner_id, template_id, clone_id, name, description, rent_amount, rent_due, privacy_option, mod_approved, last_updated, time_claimed, rejection_reason, reputation, zone_id, performance_cost FROM properties WHERE id = ? LIMIT 1;", propertyId);
	if (result.eof()) return nlohmann::json::object();
	const auto text = [&result](const char* col) { return std::string(result.fieldIsNull(col) ? "" : result.getStringField(col)); };
	const auto id = [&result](const char* col) -> nlohmann::json { return result.fieldIsNull(col) ? nlohmann::json() : nlohmann::json(std::to_string(result.getInt64Field(col))); };
	return {
		{"id", id("id")}, {"owner_id", id("owner_id")}, {"template_id", result.getIntField("template_id")}, {"clone_id", id("clone_id")},
		{"name", text("name")}, {"description", text("description")}, {"rent_amount", result.getIntField("rent_amount")}, {"rent_due", result.getInt64Field("rent_due")},
		{"privacy_option", result.getIntField("privacy_option")}, {"mod_approved", result.getIntField("mod_approved")}, {"last_updated", result.getInt64Field("last_updated")},
		{"time_claimed", result.getInt64Field("time_claimed")}, {"rejection_reason", text("rejection_reason")}, {"reputation", result.getInt64Field("reputation")},
		{"zone_id", result.getIntField("zone_id")}, {"performance_cost", result.fieldIsNull("performance_cost") ? 0.0 : result.getFloatField("performance_cost")}
	};
}

nlohmann::json SQLiteDatabase::GetPropertyModelRecords(LWOOBJID propertyId) {
	nlohmann::json rows = nlohmann::json::array();
	auto [_, result] = ExecuteSelect("SELECT pc.id, pc.property_id, pc.ugc_id, pc.lot, pc.x, pc.y, pc.z, pc.rx, pc.ry, pc.rz, pc.rw, pc.model_name, pc.model_description, "
		"pc.behavior_1, pc.behavior_2, pc.behavior_3, pc.behavior_4, pc.behavior_5, u.account_id, u.character_id AS ugc_character_id, u.is_optimized, u.bake_ao, u.filename, "
		"LENGTH(u.lxfml) AS lxfml_size, mb.ldf_config, c.name AS creator_name FROM properties_contents pc LEFT JOIN ugc u ON u.id = pc.ugc_id "
		"LEFT JOIN ugc_modular_build mb ON mb.ugc_id = pc.ugc_id LEFT JOIN charinfo c ON c.id = u.character_id WHERE pc.property_id = ? ORDER BY pc.id;", propertyId);
	for (; !result.eof(); result.nextRow()) {
		const auto text = [&result](const char* col) -> nlohmann::json { return result.fieldIsNull(col) ? nlohmann::json() : nlohmann::json(std::string(result.getStringField(col))); };
		const auto id = [&result](const char* col) -> nlohmann::json { return result.fieldIsNull(col) ? nlohmann::json() : nlohmann::json(std::to_string(result.getInt64Field(col))); };
		const auto num = [&result](const char* col) -> nlohmann::json { return result.fieldIsNull(col) ? nlohmann::json() : nlohmann::json(result.getInt64Field(col)); };
		rows.push_back({
			{"id", id("id")}, {"property_id", id("property_id")}, {"ugc_id", id("ugc_id")}, {"lot", result.getIntField("lot")},
			{"x", result.getFloatField("x")}, {"y", result.getFloatField("y")}, {"z", result.getFloatField("z")},
			{"rx", result.getFloatField("rx")}, {"ry", result.getFloatField("ry")}, {"rz", result.getFloatField("rz")}, {"rw", result.getFloatField("rw")},
			{"model_name", text("model_name")}, {"model_description", text("model_description")},
			{"behavior_1", id("behavior_1")}, {"behavior_2", id("behavior_2")}, {"behavior_3", id("behavior_3")}, {"behavior_4", id("behavior_4")}, {"behavior_5", id("behavior_5")},
			{"account_id", num("account_id")}, {"ugc_character_id", id("ugc_character_id")}, {"is_optimized", num("is_optimized")}, {"bake_ao", num("bake_ao")},
			{"filename", text("filename")}, {"lxfml_size", num("lxfml_size")}, {"ldf_config", text("ldf_config")}, {"creator_name", text("creator_name")}
		});
	}
	return rows;
}
