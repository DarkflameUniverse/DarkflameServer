#include "MySQLDatabase.h"
#include "ePropertySortType.h"

IProperty::Info ReadPropertyInfo(UniqueResultSet& result) {
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
	return info;
}

std::optional<IProperty::PropertyEntranceResult> MySQLDatabase::GetProperties(const IProperty::PropertyLookup& params) {
	std::optional<IProperty::PropertyEntranceResult> result;
	std::string query;
	std::unique_ptr<sql::ResultSet> properties;

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
			if (!result) result = IProperty::PropertyEntranceResult();
			result->totalEntriesMatchingQuery = count->getUInt("count");
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
			if (!result) result = IProperty::PropertyEntranceResult();
			result->totalEntriesMatchingQuery = count->getUInt("count");
		}
	}

	while (properties->next()) {
		if (!result) result = IProperty::PropertyEntranceResult();
		result->entries.push_back(ReadPropertyInfo(properties));
	}

	return result;
}

std::optional<IProperty::Info> MySQLDatabase::GetPropertyInfo(const LWOMAPID mapId, const LWOCLONEID cloneId) {
	auto propertyEntry = ExecuteSelect(
		"SELECT id, owner_id, clone_id, name, description, privacy_option, rejection_reason, last_updated, time_claimed, reputation, mod_approved, performance_cost "
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
		"SELECT id, owner_id, clone_id, name, description, privacy_option, rejection_reason, last_updated, time_claimed, reputation, mod_approved, performance_cost "
		"FROM properties WHERE id = ?;", id);

	if (!propertyEntry->next()) {
		return std::nullopt;
	}

	return ReadPropertyInfo(propertyEntry);
}
