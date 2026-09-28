#include "MySQLDatabase.h"

#include "UgcLookupSql.h"

namespace {
	IUgcLookup::UgcEntry ReadEntry(PreparedStmtResultSet& result, IUgcLookup::eUgcKind kind) {
		IUgcLookup::UgcEntry entry;
		entry.kind = kind;
		entry.id = result->getInt64("id");
		entry.characterId = result->getInt64("character_id");
		entry.characterName = std::string(result->getString("character_name").c_str());
		entry.accountId = static_cast<uint32_t>(result->getInt64("account_id"));
		entry.accountName = std::string(result->getString("account_name").c_str());
		entry.state = static_cast<IUgc::eProcessState>(result->getInt("is_optimized"));
		entry.error = std::string(result->getString("process_error").c_str());
		entry.detail = std::string(result->getString("detail").c_str());
		entry.attempts = static_cast<uint32_t>(result->getInt("process_attempts"));
		entry.processedAt = result->getInt64("processed_at");
		entry.processAfter = result->getInt64("process_after");
		entry.bakeAo = result->getInt("bake_ao") != 0;
		entry.bricks = static_cast<uint32_t>(result->getInt64("brick_count"));
		entry.triangles = static_cast<uint32_t>(result->getInt64("triangle_count"));
		entry.processMs = static_cast<uint32_t>(result->getInt64("process_ms"));
		return entry;
	}
}

std::vector<IUgcLookup::UgcEntry> MySQLDatabase::SearchUgc(const UgcSearch& search, const uint32_t limit) {
	std::vector<UgcEntry> entries;
	const std::string pattern = "%" + search.text + "%";
	for (const bool modular : { false, true }) {
		const auto order = modular ? "ORDER BY b.ugc_id DESC LIMIT ?;" : "ORDER BY u.id DESC LIMIT ?;";
		auto result = ExecuteSelect(UgcLookupSql::Select(modular) + UgcLookupSql::Where(search, modular) + order,
			pattern, pattern, pattern, pattern, pattern, pattern, limit);
		while (result->next()) entries.push_back(ReadEntry(result, modular ? eUgcKind::MODULAR : eUgcKind::MODEL));
	}
	return entries;
}

std::vector<IUgcLookup::UgcEntry> MySQLDatabase::GetUgcEntries(const std::vector<LWOOBJID>& ids) {
	std::vector<UgcEntry> entries;
	if (ids.empty()) return entries;
	const auto list = UgcLookupSql::IdList(ids);
	for (const bool modular : { false, true }) {
		auto result = ExecuteSelect(UgcLookupSql::Select(modular) + (modular ? "WHERE b.ugc_id IN (" : "WHERE u.id IN (") + list + ");");
		while (result->next()) entries.push_back(ReadEntry(result, modular ? eUgcKind::MODULAR : eUgcKind::MODEL));
	}
	return entries;
}

std::vector<IUgcLookup::UgcPlacement> MySQLDatabase::GetUgcPlacements(const std::vector<LWOOBJID>& ugcIds) {
	std::vector<UgcPlacement> placements;
	if (ugcIds.empty()) return placements;
	auto result = ExecuteSelect(UgcLookupSql::Placements(ugcIds));
	while (result->next()) {
		auto& placement = placements.emplace_back();
		placement.ugcId = result->getInt64("ugc_id");
		placement.modelId = result->getInt64("id");
		placement.lot = result->getInt("lot");
		placement.propertyId = result->getInt64("property_id");
		placement.propertyName = std::string(result->getString("property_name").c_str());
		placement.ownerId = result->getInt64("owner_id");
		placement.ownerName = std::string(result->getString("owner_name").c_str());
		placement.zoneId = static_cast<uint32_t>(result->getInt("zone_id"));
		placement.modelName = std::string(result->getString("model_name").c_str());
		placement.modelDescription = std::string(result->getString("model_description").c_str());
	}
	return placements;
}

std::vector<IUgcLookup::UgcMail> MySQLDatabase::GetUgcMail(const std::vector<LWOOBJID>& subkeys, const LOT modelItemLot) {
	std::vector<UgcMail> mail;
	auto result = ExecuteSelect(UgcLookupSql::Mail(subkeys, modelItemLot));
	while (result->next()) {
		auto& entry = mail.emplace_back();
		entry.id = static_cast<uint64_t>(result->getInt64("id"));
		entry.receiverId = result->getInt64("receiver_id");
		entry.receiverName = std::string(result->getString("receiver_name").c_str());
		entry.lot = result->getInt("attachment_lot");
		entry.subkey = result->getInt64("attachment_subkey");
		entry.config = std::string(result->getString("attachment_config").c_str());
	}
	return mail;
}

std::pair<std::vector<IUgcLookup::UgcEntry>, uint64_t> MySQLDatabase::ListUgc(const eUgcKind kind, const UgcListQuery& query) {
	const bool modular = kind == eUgcKind::MODULAR;
	const std::string pattern = "%" + query.search.text + "%";
	const bool searching = UgcLookupSql::Searching(query.search);
	const auto where = (searching ? UgcLookupSql::Where(query.search, modular) : std::string("WHERE 1=1 ")) + UgcLookupSql::ListFilter(query, modular);
	std::vector<UgcEntry> entries;
	uint64_t total = 0;
	const auto page = UgcLookupSql::Select(modular) + where + UgcLookupSql::ListOrder(query, modular) + "LIMIT ? OFFSET ?;";
	const auto count = "SELECT COUNT(*) AS n " + UgcLookupSql::From(modular) + where + ";";
	if (searching) {
		auto result = ExecuteSelect(page, pattern, pattern, pattern, pattern, pattern, pattern, query.limit, query.offset);
		while (result->next()) entries.push_back(ReadEntry(result, kind));
		auto counted = ExecuteSelect(count, pattern, pattern, pattern, pattern, pattern, pattern);
		if (counted->next()) total = static_cast<uint64_t>(counted->getInt64("n"));
	} else {
		auto result = ExecuteSelect(page, query.limit, query.offset);
		while (result->next()) entries.push_back(ReadEntry(result, kind));
		auto counted = ExecuteSelect(count);
		if (counted->next()) total = static_cast<uint64_t>(counted->getInt64("n"));
	}
	return { entries, total };
}
