#include "SQLiteDatabase.h"

#include "UgcLookupSql.h"

namespace {
	IUgcLookup::UgcEntry ReadEntry(CppSQLite3Query& result, IUgcLookup::eUgcKind kind) {
		IUgcLookup::UgcEntry entry;
		entry.kind = kind;
		entry.id = result.getInt64Field("id");
		entry.characterId = result.getInt64Field("character_id");
		entry.characterName = result.getStringField("character_name", "");
		entry.accountId = static_cast<uint32_t>(result.getInt64Field("account_id"));
		entry.accountName = result.getStringField("account_name", "");
		entry.state = static_cast<IUgc::eProcessState>(result.getIntField("is_optimized"));
		entry.error = result.getStringField("process_error", "");
		entry.detail = result.getStringField("detail", "");
		entry.attempts = static_cast<uint32_t>(result.getIntField("process_attempts"));
		entry.processedAt = result.getInt64Field("processed_at");
		entry.processAfter = result.getInt64Field("process_after");
		entry.bakeAo = result.getIntField("bake_ao") != 0;
		entry.bricks = static_cast<uint32_t>(result.getInt64Field("brick_count"));
		entry.triangles = static_cast<uint32_t>(result.getInt64Field("triangle_count"));
		entry.processMs = static_cast<uint32_t>(result.getInt64Field("process_ms"));
		return entry;
	}
}

std::vector<IUgcLookup::UgcEntry> SQLiteDatabase::SearchUgc(const UgcSearch& search, const uint32_t limit) {
	std::vector<UgcEntry> entries;
	const std::string pattern = "%" + search.text + "%";
	for (const bool modular : { false, true }) {
		const auto order = modular ? "ORDER BY b.ugc_id DESC LIMIT ?;" : "ORDER BY u.id DESC LIMIT ?;";
		auto [_, result] = ExecuteSelect(UgcLookupSql::Select(modular) + UgcLookupSql::Where(search, modular) + order,
			pattern, pattern, pattern, pattern, pattern, pattern, limit);
		for (; !result.eof(); result.nextRow()) entries.push_back(ReadEntry(result, modular ? eUgcKind::MODULAR : eUgcKind::MODEL));
	}
	return entries;
}

std::vector<IUgcLookup::UgcEntry> SQLiteDatabase::GetUgcEntries(const std::vector<LWOOBJID>& ids) {
	std::vector<UgcEntry> entries;
	if (ids.empty()) return entries;
	const auto list = UgcLookupSql::IdList(ids);
	for (const bool modular : { false, true }) {
		auto [_, result] = ExecuteSelect(UgcLookupSql::Select(modular) + (modular ? "WHERE b.ugc_id IN (" : "WHERE u.id IN (") + list + ");");
		for (; !result.eof(); result.nextRow()) entries.push_back(ReadEntry(result, modular ? eUgcKind::MODULAR : eUgcKind::MODEL));
	}
	return entries;
}

std::vector<IUgcLookup::UgcPlacement> SQLiteDatabase::GetUgcPlacements(const std::vector<LWOOBJID>& ugcIds) {
	std::vector<UgcPlacement> placements;
	if (ugcIds.empty()) return placements;
	auto [_, result] = ExecuteSelect(UgcLookupSql::Placements(ugcIds));
	for (; !result.eof(); result.nextRow()) {
		auto& placement = placements.emplace_back();
		placement.ugcId = result.getInt64Field("ugc_id");
		placement.modelId = result.getInt64Field("id");
		placement.lot = result.getIntField("lot");
		placement.propertyId = result.getInt64Field("property_id");
		placement.propertyName = result.getStringField("property_name", "");
		placement.ownerId = result.getInt64Field("owner_id");
		placement.ownerName = result.getStringField("owner_name", "");
		placement.zoneId = static_cast<uint32_t>(result.getIntField("zone_id"));
		placement.modelName = result.getStringField("model_name", "");
		placement.modelDescription = result.getStringField("model_description", "");
	}
	return placements;
}

std::vector<IUgcLookup::UgcMail> SQLiteDatabase::GetUgcMail(const std::vector<LWOOBJID>& subkeys, const LOT modelItemLot) {
	std::vector<UgcMail> mail;
	auto [_, result] = ExecuteSelect(UgcLookupSql::Mail(subkeys, modelItemLot));
	for (; !result.eof(); result.nextRow()) {
		auto& entry = mail.emplace_back();
		entry.id = static_cast<uint64_t>(result.getInt64Field("id"));
		entry.receiverId = result.getInt64Field("receiver_id");
		entry.receiverName = result.getStringField("receiver_name", "");
		entry.lot = result.getIntField("attachment_lot");
		entry.subkey = result.getInt64Field("attachment_subkey");
		entry.config = result.getStringField("attachment_config", "");
	}
	return mail;
}

std::pair<std::vector<IUgcLookup::UgcEntry>, uint64_t> SQLiteDatabase::ListUgc(const eUgcKind kind, const UgcListQuery& query) {
	const bool modular = kind == eUgcKind::MODULAR;
	const std::string pattern = "%" + query.search.text + "%";
	const bool searching = UgcLookupSql::Searching(query.search);
	const auto where = (searching ? UgcLookupSql::Where(query.search, modular) : std::string("WHERE 1=1 ")) + UgcLookupSql::ListFilter(query, modular);
	std::vector<UgcEntry> entries;
	uint64_t total = 0;
	const auto page = UgcLookupSql::Select(modular) + where + UgcLookupSql::ListOrder(query, modular) + "LIMIT ? OFFSET ?;";
	const auto count = "SELECT COUNT(*) AS n " + UgcLookupSql::From(modular) + where + ";";
	if (searching) {
		auto [_, result] = ExecuteSelect(page, pattern, pattern, pattern, pattern, pattern, pattern, query.limit, query.offset);
		for (; !result.eof(); result.nextRow()) entries.push_back(ReadEntry(result, kind));
		auto [__, counted] = ExecuteSelect(count, pattern, pattern, pattern, pattern, pattern, pattern);
		if (!counted.eof()) total = static_cast<uint64_t>(counted.getInt64Field("n"));
	} else {
		auto [_, result] = ExecuteSelect(page, query.limit, query.offset);
		for (; !result.eof(); result.nextRow()) entries.push_back(ReadEntry(result, kind));
		auto [__, counted] = ExecuteSelect(count);
		if (!counted.eof()) total = static_cast<uint64_t>(counted.getInt64Field("n"));
	}
	return { entries, total };
}
