#include "SQLiteDatabase.h"
#include "ePropertySortType.h"

std::optional<IProperty::PropertyEntranceResult> SQLiteDatabase::GetProperties(const IProperty::PropertyLookup& params) {
	std::optional<IProperty::PropertyEntranceResult> result;
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
			result = IProperty::PropertyEntranceResult();
			result->totalEntriesMatchingQuery = count.getIntField("count");
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
			result = IProperty::PropertyEntranceResult();
			result->totalEntriesMatchingQuery = count.getIntField("count");
		}
	}

	auto& [_, properties] = propertiesRes;
	if (!properties.eof() && !result.has_value()) result = IProperty::PropertyEntranceResult();
	while (!properties.eof()) {
		auto& entry = result->entries.emplace_back();
		entry.id = properties.getInt64Field("id");
		entry.ownerId = properties.getInt64Field("owner_id");
		entry.cloneId = properties.getInt64Field("clone_id");
		entry.name = properties.getStringField("name");
		entry.description = properties.getStringField("description");
		entry.privacyOption = properties.getIntField("privacy_option");
		entry.rejectionReason = properties.getStringField("rejection_reason");
		entry.lastUpdatedTime = properties.getIntField("last_updated");
		entry.claimedTime = properties.getIntField("time_claimed");
		entry.reputation = properties.getIntField("reputation");
		entry.modApproved = properties.getIntField("mod_approved");
		entry.performanceCost = properties.getFloatField("performance_cost");
		properties.nextRow();
	}

	return result;
}

std::optional<IProperty::Info> SQLiteDatabase::GetPropertyInfo(const LWOMAPID mapId, const LWOCLONEID cloneId) {
	auto [_, propertyEntry] = ExecuteSelect(
		"SELECT id, owner_id, clone_id, name, description, privacy_option, rejection_reason, last_updated, time_claimed, reputation, mod_approved, performance_cost "
		"FROM properties WHERE zone_id = ? AND clone_id = ?;", mapId, cloneId);

	if (propertyEntry.eof()) {
		return std::nullopt;
	}

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

	return toReturn;
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
