#include "MySQLDatabase.h"

namespace {
	std::string Text(PreparedStmtResultSet& result, const char* field) {
		return result->isNull(field) ? "" : std::string(result->getString(field).c_str());
	}

	IAiSuggestions::AiSuggestion SuggestionRow(PreparedStmtResultSet& result) {
		IAiSuggestions::AiSuggestion row;
		row.id = result->getUInt64("id");
		row.kind = Text(result, "kind");
		row.itemId = result->getInt64("item_id");
		row.fingerprint = Text(result, "fingerprint");
		row.requestedById = result->getUInt("requested_by_id");
		row.requestedBy = Text(result, "requested_by");
		row.createdAt = result->getInt64("created_at");
		row.model = Text(result, "model");
		row.inputTokens = result->getUInt("input_tokens");
		row.outputTokens = result->getUInt("output_tokens");
		row.status = static_cast<IAiSuggestions::eAiStatus>(result->getInt("status"));
		row.suggestion = Text(result, "suggestion");
		row.error = Text(result, "error");
		row.context = Text(result, "context");
		return row;
	}
}

uint64_t MySQLDatabase::InsertAiSuggestion(const AiSuggestion& s) {
	ExecuteInsert("INSERT INTO ai_suggestions (kind, item_id, fingerprint, requested_by_id, requested_by, created_at, model, input_tokens, output_tokens, status, suggestion, error, context) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		s.kind, s.itemId, s.fingerprint, s.requestedById, s.requestedBy, s.createdAt, s.model, s.inputTokens, s.outputTokens,
		static_cast<uint32_t>(s.status), s.suggestion, s.error, s.context);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return result->next() ? result->getUInt64("id") : 0;
}

std::vector<IAiSuggestions::AiSuggestion> MySQLDatabase::GetAiSuggestions(const std::string& kind, int64_t itemId, uint32_t limit) {
	auto result = ExecuteSelect("SELECT * FROM ai_suggestions WHERE kind = ? AND item_id = ? ORDER BY id DESC LIMIT ?;", kind, itemId, limit);
	std::vector<AiSuggestion> rows;
	while (result->next()) rows.push_back(SuggestionRow(result));
	return rows;
}

std::optional<IAiSuggestions::AiSuggestion> MySQLDatabase::FindAiSuggestion(const std::string& kind, int64_t itemId, const std::string& fingerprint) {
	auto result = ExecuteSelect("SELECT * FROM ai_suggestions WHERE kind = ? AND item_id = ? AND fingerprint = ? AND status = 0 ORDER BY id DESC LIMIT 1;",
		kind, itemId, fingerprint);
	if (!result->next()) return std::nullopt;
	return SuggestionRow(result);
}

IAiSuggestions::AiUsage MySQLDatabase::GetAiUsageSince(int64_t since) {
	auto result = ExecuteSelect("SELECT COUNT(*) AS requests, COALESCE(SUM(input_tokens), 0) AS input, COALESCE(SUM(output_tokens), 0) AS output "
		"FROM ai_suggestions WHERE created_at >= ?;", since);
	if (!result->next()) return {};
	return { result->getUInt("requests"), result->getUInt64("input"), result->getUInt64("output") };
}

std::optional<IAiSuggestions::EconomyFlagRow> MySQLDatabase::GetEconomyFlagRow(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM economy_flags WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	EconomyFlagRow row;
	row.id = result->getUInt64("id");
	row.createdAt = result->getInt64("created_at");
	row.day = result->getUInt("day");
	row.kind = static_cast<uint8_t>(result->getInt("kind"));
	row.characterId = result->getInt64("character_id");
	row.lot = result->getInt("lot");
	row.itemId = result->getInt64("item_id");
	row.value = result->getInt64("value");
	row.baseline = result->getInt64("baseline");
	row.details = Text(result, "details");
	row.status = static_cast<uint8_t>(result->getInt("status"));
	row.note = Text(result, "note");
	return row;
}
