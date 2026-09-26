#include "MySQLDatabase.h"
#include "GeneralUtils.h"
#include "ePropertySortType.h"

IProperty::Info ReadPropertyInfo(PreparedStmtResultSet& result) {
	IProperty::Info info;
	info.id = result->getUInt64("id");
	info.ownerId = result->getInt64("owner_id");
	info.cloneId = result->getUInt64("clone_id");
	info.name = result->getString("name").c_str();
	info.description = result->getString("description").c_str();
	info.privacyOption = result->getInt("privacy_option");
	info.rejectionReason = result->getString("rejection_reason").c_str();
	info.lastUpdatedTime = result->getUInt("last_updated");
	info.claimedTime = result->getUInt("time_claimed");
	info.reputation = result->getUInt("reputation");
	info.modApproved = result->getUInt("mod_approved");
	info.performanceCost = result->getFloat("performance_cost");
	info.zoneId = result->getUInt("zone_id");
	return info;
}

IProperty::PropertyEntranceResult MySQLDatabase::GetProperties(const IProperty::PropertyLookup& params) {
	IProperty::PropertyEntranceResult result;
	std::string query;
	PreparedStmtResultSet properties;

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
		properties = ExecuteSelect(
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
		auto count = ExecuteSelect(
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
		if (count->next()) {
			result.totalEntriesMatchingQuery = count->getUInt("count");
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
		properties = ExecuteSelect(
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
		auto count = ExecuteSelect(
			countQuery,
			params.mapId,
			"%" + params.searchString + "%",
			"%" + params.searchString + "%",
			"%" + params.searchString + "%",
			params.playerSort
		);
		if (count->next()) {
			result.totalEntriesMatchingQuery = count->getUInt("count");
		}
	}

	while (properties->next()) {
		result.entries.push_back(ReadPropertyInfo(properties));
	}

	return result;
}

std::optional<IProperty::Info> MySQLDatabase::GetPropertyInfo(const LWOMAPID mapId, const LWOCLONEID cloneId) {
	auto propertyEntry = ExecuteSelect(
		"SELECT id, owner_id, clone_id, name, description, privacy_option, rejection_reason, last_updated, time_claimed, reputation, mod_approved, performance_cost, zone_id "
		"FROM properties WHERE zone_id = ? AND clone_id = ?;", mapId, cloneId);

	if (!propertyEntry->next()) {
		return std::nullopt;
	}

	return ReadPropertyInfo(propertyEntry);
}

void MySQLDatabase::UpdatePropertyModerationInfo(const IProperty::Info& info) {
	ExecuteUpdate("UPDATE properties SET privacy_option = ?, rejection_reason = ?, mod_approved = ? WHERE id = ? LIMIT 1;",
		info.privacyOption,
		info.rejectionReason,
		info.modApproved,
		info.id);
}

void MySQLDatabase::UpdatePropertyDetails(const IProperty::Info& info) {
	ExecuteUpdate("UPDATE properties SET name = ?, description = ? WHERE id = ? LIMIT 1;", info.name, info.description, info.id);
}

void MySQLDatabase::UpdateLastSave(const IProperty::Info& info) {
	ExecuteUpdate("UPDATE properties SET last_updated = ? WHERE id = ?;", info.lastUpdatedTime, info.id);
}

void MySQLDatabase::UpdatePerformanceCost(const LWOZONEID& zoneId, const float performanceCost) {
	ExecuteUpdate("UPDATE properties SET performance_cost = ? WHERE zone_id = ? AND clone_id = ? LIMIT 1;", performanceCost, zoneId.GetMapID(), zoneId.GetCloneID());
}

void MySQLDatabase::InsertNewProperty(const IProperty::Info& info, const uint32_t templateId, const LWOZONEID& zoneId) {
	auto insertion = ExecuteInsert(
		"INSERT INTO properties"
		"(id, owner_id, template_id, clone_id, name, description, zone_id, rent_amount, rent_due, privacy_option, last_updated, time_claimed, rejection_reason, reputation, performance_cost)"
		"VALUES (?, ?, ?, ?, ?, ?, ?, 0, 0, 0, UNIX_TIMESTAMP(), UNIX_TIMESTAMP(), '', 0, 0.0)",
		info.id,
		info.ownerId,
		templateId,
		zoneId.GetCloneID(),
		info.name,
		info.description,
		zoneId.GetMapID()
	);
}

std::optional<IProperty::Info> MySQLDatabase::GetPropertyInfo(const LWOOBJID id) {
	auto propertyEntry = ExecuteSelect(
		"SELECT id, owner_id, clone_id, name, description, privacy_option, rejection_reason, last_updated, time_claimed, reputation, mod_approved, performance_cost, zone_id "
		"FROM properties WHERE id = ?;", id);

	if (!propertyEntry->next()) {
		return std::nullopt;
	}

	return ReadPropertyInfo(propertyEntry);
}

#include "json.hpp"

uint32_t MySQLDatabase::GetPropertyCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM properties;");
	return res->next() ? res->getUInt("count") : 0;
}

std::string MySQLDatabase::GetPropertiesTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc, bool pendingOnly) {
	// A number in the search box also matches IDs exactly (-1 never matches)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	// Build base query
	std::string baseQuery = "SELECT id, owner_id, name, mod_approved, reputation, performance_cost, zone_id, (SELECT c.name FROM charinfo c WHERE c.id = properties.owner_id) AS owner_name, (SELECT COUNT(*) FROM properties_contents pc WHERE pc.property_id = properties.id) AS models FROM properties";
	std::string whereClause;
	std::string orderClause;

	// Only public properties awaiting review show up in the moderation queue
	const std::string pendingFilter = pendingOnly ? "mod_approved = 0 AND privacy_option = 2 AND rejection_reason = ''" : "";
	const std::string searchFilter = !search.empty() ? "(name LIKE CONCAT('%', ?, '%') OR owner_id IN (SELECT id FROM charinfo WHERE name LIKE CONCAT('%', ?, '%')) OR id = ? OR owner_id = ?)" : "";
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
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ?, ?;";

	// Get total count
	std::string totalCountQuery = "SELECT COUNT(*) as count FROM properties" + (pendingOnly ? " WHERE " + pendingFilter : std::string{}) + ";";
	auto totalCountResult = ExecuteSelect(totalCountQuery);
	uint32_t totalRecords = totalCountResult->next() ? totalCountResult->getUInt("count") : 0;

	// Get filtered count
	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		std::string filteredCountQuery = "SELECT COUNT(*) as count FROM properties" + whereClause + ";";
		auto filteredCountResult = ExecuteSelect(filteredCountQuery, search, search, searchId, searchId);
		filteredRecords = filteredCountResult->next() ? filteredCountResult->getUInt("count") : 0;
	}

	// Execute main query
	auto result = !search.empty()
		? ExecuteSelect(mainQuery, search, search, searchId, searchId, start, length)
		: ExecuteSelect(mainQuery, start, length);

	// Build response JSON
	nlohmann::json propertiesArray = nlohmann::json::array();

	while (result->next()) {
		nlohmann::json property = {
			{"id", std::to_string(result->getUInt64("id"))},
			{"owner_id", std::to_string(result->getUInt64("owner_id"))},
			{"owner_name", result->isNull("owner_name") ? "" : std::string(result->getString("owner_name").c_str())},
			{"name", result->getString("name")},
			{"mod_approved", result->getBoolean("mod_approved")},
			{"reputation", result->getUInt64("reputation")},
			{"performance_cost", result->isNull("performance_cost") ? 0.0 : static_cast<double>(result->getDouble("performance_cost"))},
			{"models", result->getUInt("models")},
			{"zone_id", result->getUInt("zone_id")}
		};
		propertiesArray.push_back(property);
	}

	nlohmann::json response = {
		{"draw", 0},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", propertiesArray}
	};

	return response.dump();
}

void MySQLDatabase::ApproveProperty(const LWOOBJID propertyId) {
	ExecuteUpdate("UPDATE properties SET mod_approved = 1 WHERE id = ?;", propertyId);
}

IProperty::ShowcaseResult MySQLDatabase::GetShowcaseProperties(const IProperty::ShowcaseQuery& query) {
	// Public (privacy_option 2) and approved only; the owner must still exist, since the showcase names them
	const std::string from =
		" FROM properties p JOIN charinfo c ON c.id = p.owner_id"
		" WHERE p.mod_approved = 1 AND p.privacy_option = 2"
		" AND (? = 0 OR p.zone_id = ?)"
		" AND (? = '' OR p.name LIKE CONCAT('%', ?, '%') ESCAPE '!' OR p.description LIKE CONCAT('%', ?, '%') ESCAPE '!' OR c.name LIKE CONCAT('%', ?, '%') ESCAPE '!')";
	std::string order;
	switch (query.sort) {
	case ShowcaseSort::NEWEST: order = " ORDER BY p.last_updated DESC, p.id DESC"; break;
	case ShowcaseSort::NAME: order = " ORDER BY p.name ASC, p.id ASC"; break;
	default: order = " ORDER BY p.reputation DESC, p.last_updated DESC, p.id DESC"; break;
	}

	IProperty::ShowcaseResult showcase;
	const auto& s = query.search;
	const auto like = GeneralUtils::LikeEscape(query.search, '!'); // a % or _ typed in the search box is matched literally
	auto count = ExecuteSelect("SELECT COUNT(*) AS count" + from + ";", query.zoneId, query.zoneId, s, like, like, like);
	showcase.total = count->next() ? count->getUInt("count") : 0;

	auto result = ExecuteSelect("SELECT p.*, c.name AS owner_name, (SELECT COUNT(*) FROM properties_contents pc WHERE pc.property_id = p.id) AS models" +
		from + order + " LIMIT ?, ?;", query.zoneId, query.zoneId, s, like, like, like, query.start, query.length);
	while (result->next()) {
		showcase.entries.push_back({ ReadPropertyInfo(result), result->getString("owner_name").c_str(), result->getUInt("models") });
	}
	return showcase;
}
