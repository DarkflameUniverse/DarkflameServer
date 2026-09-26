#include "SQLiteDatabase.h"

namespace {
	std::string Text(CppSQLite3Query& result, const char* field) {
		return result.fieldIsNull(field) ? "" : result.getStringField(field);
	}

	IAiSuggestions::AiSuggestion SuggestionRow(CppSQLite3Query& result) {
		IAiSuggestions::AiSuggestion row;
		row.id = static_cast<uint64_t>(result.getInt64Field("id"));
		row.kind = Text(result, "kind");
		row.itemId = result.getInt64Field("item_id");
		row.fingerprint = Text(result, "fingerprint");
		row.requestedById = static_cast<uint32_t>(result.getInt64Field("requested_by_id"));
		row.requestedBy = Text(result, "requested_by");
		row.createdAt = result.getInt64Field("created_at");
		row.model = Text(result, "model");
		row.inputTokens = static_cast<uint32_t>(result.getInt64Field("input_tokens"));
		row.outputTokens = static_cast<uint32_t>(result.getInt64Field("output_tokens"));
		row.status = static_cast<IAiSuggestions::eAiStatus>(result.getIntField("status"));
		row.suggestion = Text(result, "suggestion");
		row.error = Text(result, "error");
		row.context = Text(result, "context");
		return row;
	}
}

uint64_t SQLiteDatabase::InsertAiSuggestion(const AiSuggestion& s) {
	ExecuteInsert("INSERT INTO ai_suggestions (kind, item_id, fingerprint, requested_by_id, requested_by, created_at, model, input_tokens, output_tokens, status, suggestion, error, context) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		s.kind, s.itemId, s.fingerprint, s.requestedById, s.requestedBy, s.createdAt, s.model, s.inputTokens, s.outputTokens,
		static_cast<uint32_t>(s.status), s.suggestion, s.error, s.context);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("id"));
}

std::vector<IAiSuggestions::AiSuggestion> SQLiteDatabase::GetAiSuggestions(const std::string& kind, int64_t itemId, uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT * FROM ai_suggestions WHERE kind = ? AND item_id = ? ORDER BY id DESC LIMIT ?;", kind, itemId, limit);
	std::vector<AiSuggestion> rows;
	for (; !result.eof(); result.nextRow()) rows.push_back(SuggestionRow(result));
	return rows;
}

std::optional<IAiSuggestions::AiSuggestion> SQLiteDatabase::FindAiSuggestion(const std::string& kind, int64_t itemId, const std::string& fingerprint) {
	auto [_, result] = ExecuteSelect("SELECT * FROM ai_suggestions WHERE kind = ? AND item_id = ? AND fingerprint = ? AND status = 0 ORDER BY id DESC LIMIT 1;",
		kind, itemId, fingerprint);
	if (result.eof()) return std::nullopt;
	return SuggestionRow(result);
}

IAiSuggestions::AiUsage SQLiteDatabase::GetAiUsageSince(int64_t since) {
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS requests, COALESCE(SUM(input_tokens), 0) AS input, COALESCE(SUM(output_tokens), 0) AS output "
		"FROM ai_suggestions WHERE created_at >= ?;", since);
	if (result.eof()) return {};
	return { static_cast<uint32_t>(result.getInt64Field("requests")), static_cast<uint64_t>(result.getInt64Field("input")),
		static_cast<uint64_t>(result.getInt64Field("output")) };
}

std::optional<IAiSuggestions::EconomyFlagRow> SQLiteDatabase::GetEconomyFlagRow(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM economy_flags WHERE id = ?;", static_cast<int64_t>(id));
	if (result.eof()) return std::nullopt;
	EconomyFlagRow row;
	row.id = static_cast<uint64_t>(result.getInt64Field("id"));
	row.createdAt = result.getInt64Field("created_at");
	row.day = static_cast<uint32_t>(result.getIntField("day"));
	row.kind = static_cast<uint8_t>(result.getIntField("kind"));
	row.characterId = result.getInt64Field("character_id");
	row.lot = result.getIntField("lot");
	row.itemId = result.getInt64Field("item_id");
	row.value = result.getInt64Field("value");
	row.baseline = result.getInt64Field("baseline");
	row.details = Text(result, "details");
	row.status = static_cast<uint8_t>(result.getIntField("status"));
	row.note = Text(result, "note");
	return row;
}
