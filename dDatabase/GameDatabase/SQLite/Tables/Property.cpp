#include "SQLiteDatabase.h"
#include "GeneralUtils.h"
#include "ePropertySortType.h"

IProperty::Info ReadPropertyInfo(CppSQLite3Query& propertyEntry) {
	IProperty::Info toReturn;
	toReturn.id = propertyEntry.getInt64Field("id");
	toReturn.ownerId = propertyEntry.getInt64Field("owner_id");
	toReturn.cloneId = propertyEntry.getInt64Field("clone_id");
	toReturn.name = propertyEntry.getStringField("name");
	toReturn.description = propertyEntry.getStringField("description");
	toReturn.privacyOption = propertyEntry.getIntField("privacy_option");
	toReturn.rejectionReason = propertyEntry.getStringField("rejection_reason");
	toReturn.lastUpdatedTime = propertyEntry.getIntField("last_updated");
	toReturn.claimedTime = propertyEntry.getIntField("time_claimed");
	toReturn.reputation = propertyEntry.getIntField("reputation");
	toReturn.modApproved = propertyEntry.getIntField("mod_approved");
	toReturn.performanceCost = propertyEntry.getFloatField("performance_cost");
	toReturn.zoneId = static_cast<uint32_t>(propertyEntry.getIntField("zone_id"));
	return toReturn;
}

IProperty::PropertyEntranceResult SQLiteDatabase::GetProperties(const IProperty::PropertyLookup& params) {
	IProperty::PropertyEntranceResult result;
	std::string query;
	std::pair<CppSQLite3Statement, CppSQLite3Query> propertiesRes;

	if (params.sortChoice == SORT_TYPE_FEATURED || params.sortChoice == SORT_TYPE_FRIENDS) {
		query = R"QUERY(
		FROM properties as p
		JOIN charinfo as ci
		ON ci.prop_clone_id = p.clone_id
		where p.zone_id = ?
		AND (
			p.description LIKE ?
		    OR p.name LIKE ?
		    OR ci.name LIKE ?
		)
		AND p.privacy_option >= ?
		AND p.owner_id IN (
			SELECT fr.requested_player AS player FROM (
				SELECT CASE 
				WHEN player_id = ? THEN friend_id 
				WHEN friend_id = ? THEN player_id 
				END AS requested_player FROM friends
			) AS fr 
			JOIN charinfo AS ci ON ci.id = fr.requested_player 
			WHERE fr.requested_player IS NOT NULL AND fr.requested_player != ?
		) ORDER BY ci.name ASC
		)QUERY";
		const auto completeQuery = "SELECT p.* " + query + " LIMIT ? OFFSET ?;";
		propertiesRes = ExecuteSelect(
			completeQuery,
			params.mapId,
			"%" + params.searchString + "%",
			"%" + params.searchString + "%",
			"%" + params.searchString + "%",
			params.playerSort,
			params.playerId,
			params.playerId,
			params.playerId,
			params.numResults,
			params.startIndex
		);
		const auto countQuery = "SELECT COUNT(*) as count" + query + ";";
		auto [_, count] = ExecuteSelect(
			countQuery,
			params.mapId,
			"%" + params.searchString + "%",
			"%" + params.searchString + "%",
			"%" + params.searchString + "%",
			params.playerSort,
			params.playerId,
			params.playerId,
			params.playerId
		);
		if (!count.eof()) {
			result.totalEntriesMatchingQuery = count.getIntField("count");
		}
	} else {
		if (params.sortChoice == SORT_TYPE_REPUTATION) {
			query = R"QUERY(
			FROM properties as p
			JOIN charinfo as ci
			ON ci.prop_clone_id = p.clone_id
			where p.zone_id = ?
			AND (
				p.description LIKE ?
			    OR p.name LIKE ?
			    OR ci.name LIKE ?
			)
			AND p.privacy_option >= ?
			ORDER BY p.reputation DESC, p.last_updated DESC 
			)QUERY";
		} else {
			query = R"QUERY(
			FROM properties as p
			JOIN charinfo as ci
			ON ci.prop_clone_id = p.clone_id
			where p.zone_id = ?
			AND (
				p.description LIKE ?
			    OR p.name LIKE ?
			    OR ci.name LIKE ?
			)
			AND p.privacy_option >= ?
			ORDER BY p.last_updated DESC
			)QUERY";
		}
		const auto completeQuery = "SELECT p.* " + query + " LIMIT ? OFFSET ?;";
		propertiesRes = ExecuteSelect(
			completeQuery,
			params.mapId,
			"%" + params.searchString + "%",
			"%" + params.searchString + "%",
			"%" + params.searchString + "%",
			params.playerSort,
			params.numResults,
			params.startIndex
		);
		const auto countQuery = "SELECT COUNT(*) as count" + query + ";";
		auto [_, count] = ExecuteSelect(
			countQuery,
			params.mapId,
			"%" + params.searchString + "%",
			"%" + params.searchString + "%",
			"%" + params.searchString + "%",
			params.playerSort
		);
		if (!count.eof()) {
			result.totalEntriesMatchingQuery = count.getIntField("count");
		}
	}

	auto& [_, properties] = propertiesRes;
	while (!properties.eof()) {
		result.entries.push_back(ReadPropertyInfo(properties));
		properties.nextRow();
	}

	return result;
}

std::optional<IProperty::Info> SQLiteDatabase::GetPropertyInfo(const LWOMAPID mapId, const LWOCLONEID cloneId) {
	auto [_, propertyEntry] = ExecuteSelect(
		"SELECT id, owner_id, clone_id, name, description, privacy_option, rejection_reason, last_updated, time_claimed, reputation, mod_approved, performance_cost, zone_id "
		"FROM properties WHERE zone_id = ? AND clone_id = ?;", mapId, cloneId);

	if (propertyEntry.eof()) {
		return std::nullopt;
	}

	return ReadPropertyInfo(propertyEntry);
}

void SQLiteDatabase::UpdatePropertyModerationInfo(const IProperty::Info& info) {
	ExecuteUpdate("UPDATE properties SET privacy_option = ?, rejection_reason = ?, mod_approved = ? WHERE id = ?;",
		info.privacyOption,
		info.rejectionReason,
		info.modApproved,
		info.id);
}

void SQLiteDatabase::UpdatePropertyDetails(const IProperty::Info& info) {
	ExecuteUpdate("UPDATE properties SET name = ?, description = ? WHERE id = ?;", info.name, info.description, info.id);
}

void SQLiteDatabase::UpdateLastSave(const IProperty::Info& info) {
	ExecuteUpdate("UPDATE properties SET last_updated = ? WHERE id = ?;", info.lastUpdatedTime, info.id);
}

void SQLiteDatabase::UpdatePerformanceCost(const LWOZONEID& zoneId, const float performanceCost) {
	ExecuteUpdate("UPDATE properties SET performance_cost = ? WHERE zone_id = ? AND clone_id = ?;", performanceCost, zoneId.GetMapID(), zoneId.GetCloneID());
}

void SQLiteDatabase::InsertNewProperty(const IProperty::Info& info, const uint32_t templateId, const LWOZONEID& zoneId) {
	auto insertion = ExecuteInsert(
		"INSERT INTO properties"
		" (id, owner_id, template_id, clone_id, name, description, zone_id, rent_amount, rent_due, privacy_option, last_updated, time_claimed, rejection_reason, reputation, performance_cost)"
		" VALUES (?, ?, ?, ?, ?, ?, ?, 0, 0, 0, CAST(strftime('%s', 'now') as INT), CAST(strftime('%s', 'now') as INT), '', 0, 0.0)",
		info.id,
		info.ownerId,
		templateId,
		zoneId.GetCloneID(),
		info.name,
		info.description,
		zoneId.GetMapID()
	);
}

std::optional<IProperty::Info> SQLiteDatabase::GetPropertyInfo(const LWOOBJID id) {
	auto [_, propertyEntry] = ExecuteSelect(
		"SELECT id, owner_id, clone_id, name, description, privacy_option, rejection_reason, last_updated, time_claimed, reputation, mod_approved, performance_cost, zone_id "
		"FROM properties WHERE id = ?;", id);
	
	if (propertyEntry.eof()) {
		return std::nullopt;
	}

	return ReadPropertyInfo(propertyEntry);
}

#include "json.hpp"

uint32_t SQLiteDatabase::GetPropertyCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM properties;");
	if (res.eof()) return 0;
	return res.getIntField("count");
}

std::string SQLiteDatabase::GetPropertiesTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc, bool pendingOnly) {
	// A number in the search box also matches IDs exactly (-1 never matches)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	// Build base query
	std::string baseQuery = "SELECT id, owner_id, name, mod_approved, reputation, performance_cost, zone_id, (SELECT c.name FROM charinfo c WHERE c.id = properties.owner_id) AS owner_name, (SELECT COUNT(*) FROM properties_contents pc WHERE pc.property_id = properties.id) AS models FROM properties";
	std::string whereClause;
	std::string orderClause;

	// Only public properties awaiting review show up in the moderation queue
	const std::string pendingFilter = pendingOnly ? "mod_approved = 0 AND privacy_option = 2 AND rejection_reason = ''" : "";
	const std::string searchFilter = !search.empty() ? "(name LIKE '%' || ? || '%' OR owner_id IN (SELECT id FROM charinfo WHERE name LIKE '%' || ? || '%') OR id = ? OR owner_id = ?)" : "";
	if (!pendingFilter.empty() && !searchFilter.empty()) whereClause = " WHERE " + pendingFilter + " AND " + searchFilter;
	else if (!pendingFilter.empty()) whereClause = " WHERE " + pendingFilter;
	else if (!searchFilter.empty()) whereClause = " WHERE " + searchFilter;

	// Map column indices to database columns
	std::string orderColumnName = "id";
	switch (orderColumn) {
		case 0: orderColumnName = "id"; break;
		case 1: orderColumnName = "name"; break;
		case 2: orderColumnName = "owner_id"; break;
		case 3: orderColumnName = "mod_approved"; break;
		case 4: orderColumnName = "reputation"; break;
		case 5: orderColumnName = "performance_cost"; break;
		case 6: orderColumnName = "zone_id"; break;
		default: orderColumnName = "id";
	}

	orderClause = " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC");

	// Build the main query
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ? OFFSET ?;";

	// Get total count
	std::string totalCountQuery = "SELECT COUNT(*) as count FROM properties" + (pendingOnly ? " WHERE " + pendingFilter : std::string{}) + ";";
	auto [__, totalCountResult] = ExecuteSelect(totalCountQuery);
	uint32_t totalRecords = totalCountResult.eof() ? 0 : totalCountResult.getIntField("count");

	// Get filtered count
	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		std::string filteredCountQuery = "SELECT COUNT(*) as count FROM properties" + whereClause + ";";
		auto [___, filteredCountResult] = ExecuteSelect(filteredCountQuery, search, search, searchId, searchId);
		filteredRecords = filteredCountResult.eof() ? 0 : filteredCountResult.getIntField("count");
	}

	// Execute main query
	auto [stmt, result] = !search.empty() ? 
		ExecuteSelect(mainQuery, search, search, searchId, searchId, length, start) :
		ExecuteSelect(mainQuery, length, start);

	// Build response JSON
	nlohmann::json propertiesArray = nlohmann::json::array();

	while (!result.eof()) {
		nlohmann::json property = {
			{"id", std::to_string(result.getInt64Field("id"))},
			{"owner_id", std::to_string(result.getInt64Field("owner_id"))},
			{"owner_name", result.fieldIsNull("owner_name") ? "" : result.getStringField("owner_name")},
			{"name", result.getStringField("name")},
			{"mod_approved", result.getIntField("mod_approved") != 0},
			{"reputation", result.getInt64Field("reputation")},
			{"performance_cost", result.fieldIsNull("performance_cost") ? 0.0 : result.getFloatField("performance_cost")},
			{"models", result.getIntField("models")},
			{"zone_id", result.getIntField("zone_id")}
		};
		propertiesArray.push_back(property);
		result.nextRow();
	}

	nlohmann::json response = {
		{"draw", 0},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", propertiesArray}
	};

	return response.dump();
}

void SQLiteDatabase::ApproveProperty(const LWOOBJID propertyId) {
	ExecuteUpdate("UPDATE properties SET mod_approved = 1 WHERE id = ?;", propertyId);
}

IProperty::ShowcaseResult SQLiteDatabase::GetShowcaseProperties(const IProperty::ShowcaseQuery& query) {
	// Public (privacy_option 2) and approved only; the owner must still exist, since the showcase names them
	const std::string from =
		" FROM properties p JOIN charinfo c ON c.id = p.owner_id"
		" WHERE p.mod_approved = 1 AND p.privacy_option = 2"
		" AND (? = 0 OR p.zone_id = ?)"
		" AND (? = '' OR p.name LIKE '%' || ? || '%' ESCAPE '!' OR p.description LIKE '%' || ? || '%' ESCAPE '!' OR c.name LIKE '%' || ? || '%' ESCAPE '!')";
	std::string order;
	switch (query.sort) {
	case ShowcaseSort::NEWEST: order = " ORDER BY p.last_updated DESC, p.id DESC"; break;
	case ShowcaseSort::NAME: order = " ORDER BY p.name COLLATE NOCASE ASC, p.id ASC"; break;
	default: order = " ORDER BY p.reputation DESC, p.last_updated DESC, p.id DESC"; break;
	}

	IProperty::ShowcaseResult showcase;
	const auto& s = query.search;
	const auto like = GeneralUtils::LikeEscape(query.search, '!'); // a % or _ typed in the search box is matched literally
	auto [countStmt, count] = ExecuteSelect("SELECT COUNT(*) AS count" + from + ";", query.zoneId, query.zoneId, s, like, like, like);
	showcase.total = count.eof() ? 0 : static_cast<uint32_t>(count.getIntField("count"));

	auto [stmt, result] = ExecuteSelect("SELECT p.*, c.name AS owner_name, (SELECT COUNT(*) FROM properties_contents pc WHERE pc.property_id = p.id) AS models" +
		from + order + " LIMIT ? OFFSET ?;", query.zoneId, query.zoneId, s, like, like, like, query.length, query.start);
	for (; !result.eof(); result.nextRow()) {
		showcase.entries.push_back({ ReadPropertyInfo(result), result.getStringField("owner_name"), static_cast<uint32_t>(result.getIntField("models")) });
	}
	return showcase;
}
