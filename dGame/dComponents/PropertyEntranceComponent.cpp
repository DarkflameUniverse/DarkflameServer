#include "PropertyEntranceComponent.h"

#include <CDPropertyEntranceComponentTable.h>

#include "Character.h"
#include "Database.h"
#include "GameMessages.h"
#include "PropertyManagementComponent.h"
#include "PropertySelectQueryProperty.h"
#include "RocketLaunchpadControlComponent.h"
#include "CharacterComponent.h"
#include "UserManager.h"
#include "dLogger.h"

PropertyEntranceComponent::PropertyEntranceComponent(uint32_t componentID, Entity* parent) : Component(parent) {
	this->propertyQueries = {};

	auto table = CDClientManager::Instance()->GetTable<CDPropertyEntranceComponentTable>("PropertyEntranceComponent");
	const auto& entry = table->GetByID(componentID);

	this->m_MapID = entry.mapID;
	this->m_PropertyName = entry.propertyName;
}

void PropertyEntranceComponent::OnUse(Entity* entity) {
	auto* characterComponent = entity->GetComponent<CharacterComponent>();
	if (!characterComponent) return;

	auto* rocket = entity->GetComponent<CharacterComponent>()->RocketEquip(entity);
	if (!rocket) return;

	GameMessages::SendPropertyEntranceBegin(m_Parent->GetObjectID(), entity->GetSystemAddress());

	AMFArrayValue args;

	auto* state = new AMFStringValue();
	state->SetStringValue("property_menu");

	args.InsertValue("state", state);

	GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "pushGameState", &args);
}

void PropertyEntranceComponent::OnEnterProperty(Entity* entity, uint32_t index, bool returnToZone, const SystemAddress& sysAddr) {
	LWOCLONEID cloneId = 0;

	if (index == -1 && !returnToZone) {
		cloneId = entity->GetCharacter()->GetPropertyCloneID();
	} else if (index == -1 && returnToZone) {
		cloneId = 0;
	} else if (index >= 0) {
		// Increment index once here because the first index of other player properties is 2 in the propertyQueries cache.
		index++;

		const auto& pair = propertyQueries.find(entity->GetObjectID());

		if (pair == propertyQueries.end()) return;

		const auto& query = pair->second;

		if (index >= query.size()) return;

		cloneId = query[index].CloneId;
	}

	auto* launcher = m_Parent->GetComponent<RocketLaunchpadControlComponent>();

	if (launcher == nullptr) {
		return;
	}

	launcher->SetSelectedCloneId(entity->GetObjectID(), cloneId);

	launcher->Launch(entity, launcher->GetTargetZone(), cloneId);
}

PropertySelectQueryProperty PropertyEntranceComponent::SetPropertyValues(PropertySelectQueryProperty property, LWOCLONEID cloneId, std::string ownerName, std::string propertyName, std::string propertyDescription, float reputation, bool isBFF, bool isFriend, bool isModeratorApproved, bool isAlt, bool isOwned, uint32_t privacyOption, uint32_t timeLastUpdated, float performanceCost) {
	property.CloneId = cloneId;
	property.OwnerName = ownerName;
	property.Name = propertyName;
	property.Description = propertyDescription;
	property.Reputation = reputation;
	property.IsBestFriend = isBFF;
	property.IsFriend = isFriend;
	property.IsModeratorApproved = isModeratorApproved;
	property.IsAlt = isAlt;
	property.IsOwned = isOwned;
	property.AccessType = privacyOption;
	property.DateLastPublished = timeLastUpdated;
	property.PerformanceCost = performanceCost;

	return property;
}

std::string PropertyEntranceComponent::BuildQuery(Entity* entity, int32_t sortMethod, Character* character, std::string customQuery, bool wantLimits) {
	std::string base;
	if (customQuery == "") {
		base = baseQueryForProperties;
	} else {
		base = customQuery;
	}
	std::string orderBy = "";
	if (sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS) {
		std::string friendsList = " AND p.owner_id IN (";

		auto friendsListQuery = Database::CreatePreppedStmt("SELECT * FROM (SELECT CASE WHEN player_id = ? THEN friend_id WHEN friend_id = ? THEN player_id END AS requested_player FROM friends ) AS fr WHERE requested_player IS NOT NULL ORDER BY requested_player DESC;");

		friendsListQuery->setUInt(1, character->GetID());
		friendsListQuery->setUInt(2, character->GetID());

		auto friendsListQueryResult = friendsListQuery->executeQuery();

		while (friendsListQueryResult->next()) {
			auto playerIDToConvert = friendsListQueryResult->getInt(1);
			friendsList = friendsList + std::to_string(playerIDToConvert) + ",";
		}
		// Replace trailing comma with the closing parenthesis.
		if (friendsList.at(friendsList.size() - 1) == ',') friendsList.erase(friendsList.size() - 1, 1);
		friendsList += ") ";

		// If we have no friends then use a -1 for the query.
		if (friendsList.find("()") != std::string::npos) friendsList = " AND p.owner_id IN (-1) ";

		orderBy += friendsList + "ORDER BY ci.name ASC ";

		delete friendsListQueryResult;
		friendsListQueryResult = nullptr;

		delete friendsListQuery;
		friendsListQuery = nullptr;
	} else if (sortMethod == SORT_TYPE_RECENT) {
		orderBy = "ORDER BY p.last_updated DESC ";
	} else if (sortMethod == SORT_TYPE_REPUTATION) {
		orderBy = "ORDER BY p.reputation DESC, p.last_updated DESC ";
	} else {
		orderBy = "ORDER BY p.last_updated DESC ";
	}
	return base + orderBy + (wantLimits ? "LIMIT ? OFFSET ?;" : ";");
}

void PropertyEntranceComponent::OnPropertyEntranceSync(Entity* entity, bool includeNullAddress, bool includeNullDescription, bool playerOwn, bool updateUi, int32_t numResults, int32_t lReputationTime, int32_t sortMethod, int32_t startIndex, std::string filterText, const SystemAddress& sysAddr) {

	std::vector<PropertySelectQueryProperty> entries{};
	PropertySelectQueryProperty playerEntry{};

	auto character = entity->GetCharacter();
	if (!character) return;

	// Player property goes in index 1 of the vector.  This is how the client expects it.
	auto playerPropertyLookup = Database::CreatePreppedStmt("SELECT * FROM properties WHERE owner_id = ? AND zone_id = ?");

	playerPropertyLookup->setInt(1, character->GetID());
	playerPropertyLookup->setInt(2, this->m_MapID);

	auto playerPropertyLookupResults = playerPropertyLookup->executeQuery();

	// If the player has a property this query will have a single result.
	if (playerPropertyLookupResults->next()) {
		const auto cloneId = playerPropertyLookupResults->getUInt64(4);
		const auto propertyName = std::string(playerPropertyLookupResults->getString(5).c_str());
		const auto propertyDescription = std::string(playerPropertyLookupResults->getString(6).c_str());
		const auto privacyOption = playerPropertyLookupResults->getInt(9);
		const auto modApproved = playerPropertyLookupResults->getBoolean(10);
		const auto dateLastUpdated = playerPropertyLookupResults->getInt64(11);
		const auto reputation = playerPropertyLookupResults->getUInt(14);
		const auto performanceCost = (float)playerPropertyLookupResults->getDouble(16);

		playerEntry = SetPropertyValues(playerEntry, cloneId, character->GetName(), propertyName, propertyDescription, reputation, true, true, modApproved, true, true, privacyOption, dateLastUpdated, performanceCost);
	} else {
		playerEntry = SetPropertyValues(playerEntry, character->GetPropertyCloneID(), character->GetName(), "", "", 0, true, true);
	}

	delete playerPropertyLookupResults;
	playerPropertyLookupResults = nullptr;

	delete playerPropertyLookup;
	playerPropertyLookup = nullptr;

	entries.push_back(playerEntry);

	const auto query = BuildQuery(entity, sortMethod, character);

	auto propertyLookup = Database::CreatePreppedStmt(query);

	const auto searchString = "%" + filterText + "%";
	propertyLookup->setUInt(1, this->m_MapID);
	propertyLookup->setString(2, searchString.c_str());
	propertyLookup->setString(3, searchString.c_str());
	propertyLookup->setString(4, searchString.c_str());
	propertyLookup->setInt(5, sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS ? (uint32_t)PropertyPrivacyOption::Friends : (uint32_t)PropertyPrivacyOption::Public);
	propertyLookup->setInt(6, numResults);
	propertyLookup->setInt(7, startIndex);

	auto propertyEntry = propertyLookup->executeQuery();

	while (propertyEntry->next()) {
		const auto propertyId = propertyEntry->getUInt64(1);
		const auto owner = propertyEntry->getInt(2);
		const auto cloneId = propertyEntry->getUInt64(4);
		const auto propertyNameFromDb = std::string(propertyEntry->getString(5).c_str());
		const auto propertyDescriptionFromDb = std::string(propertyEntry->getString(6).c_str());
		const auto privacyOption = propertyEntry->getInt(9);
		const auto modApproved = propertyEntry->getBoolean(10);
		const auto dateLastUpdated = propertyEntry->getInt(11);
		const float reputation = propertyEntry->getInt(14);
		const auto performanceCost = (float)propertyEntry->getDouble(16);

		PropertySelectQueryProperty entry{};

		std::string ownerName = "";
		bool isOwned = true;
		auto nameLookup = Database::CreatePreppedStmt("SELECT name FROM charinfo WHERE prop_clone_id = ?;");

		nameLookup->setUInt64(1, cloneId);

		auto nameResult = nameLookup->executeQuery();

		if (!nameResult->next()) {
			delete nameLookup;
			nameLookup = nullptr;

			Game::logger->Log("PropertyEntranceComponent", "Failed to find property owner name for %llu!", cloneId);

			continue;
		} else {
			isOwned = cloneId == character->GetPropertyCloneID();
			ownerName = std::string(nameResult->getString(1).c_str());
		}

		delete nameResult;
		nameResult = nullptr;

		delete nameLookup;
		nameLookup = nullptr;

		std::string propertyName = propertyNameFromDb;
		std::string propertyDescription = propertyDescriptionFromDb;

		bool isBestFriend = false;
		bool isFriend = false;

		// Convert owner char id to LWOOBJID
		LWOOBJID ownerObjId = owner;
		ownerObjId = GeneralUtils::SetBit(ownerObjId, OBJECT_BIT_CHARACTER);
		ownerObjId = GeneralUtils::SetBit(ownerObjId, OBJECT_BIT_PERSISTENT);

		// Query to get friend and best friend fields
		auto friendCheck = Database::CreatePreppedStmt("SELECT best_friend FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?)");

		friendCheck->setUInt(1, character->GetID());
		friendCheck->setUInt(2, ownerObjId);
		friendCheck->setUInt(3, ownerObjId);
		friendCheck->setUInt(4, character->GetID());

		auto friendResult = friendCheck->executeQuery();

		// If we got a result than the two players are friends.
		if (friendResult->next()) {
			isFriend = true;
			if (friendResult->getInt(1) == 3) {
				isBestFriend = true;
			}
		}

		delete friendCheck;
		friendCheck = nullptr;

		delete friendResult;
		friendResult = nullptr;

		bool isModeratorApproved = propertyEntry->getBoolean(10);

		if (!isModeratorApproved && entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR) {
			propertyName = "[AWAITING APPROVAL]";
			propertyDescription = "[AWAITING APPROVAL]";
			isModeratorApproved = true;
		}

		bool isAlt = false;
		// Query to determine whether this property is an alt character of the entity.
		auto isAltQuery = Database::CreatePreppedStmt("SELECT id FROM charinfo where account_id in (SELECT account_id from charinfo WHERE id = ?) AND id = ?;");

		isAltQuery->setInt(1, character->GetID());
		isAltQuery->setInt(2, owner);

		auto isAltQueryResults = isAltQuery->executeQuery();

		if (isAltQueryResults->next()) {
			isAlt = true;
		}

		delete isAltQueryResults;
		isAltQueryResults = nullptr;

		delete isAltQuery;
		isAltQuery = nullptr;

		entry = SetPropertyValues(entry, cloneId, ownerName, propertyName, propertyDescription, reputation, isBestFriend, isFriend, isModeratorApproved, isAlt, isOwned, privacyOption, dateLastUpdated, performanceCost);

		entries.push_back(entry);
	}

	delete propertyEntry;
	propertyEntry = nullptr;

	delete propertyLookup;
	propertyLookup = nullptr;

	propertyQueries[entity->GetObjectID()] = entries;

	// Query here is to figure out whether or not to display the button to go to the next page or not.
	int32_t numberOfProperties = 0;

	auto buttonQuery = BuildQuery(entity, sortMethod, character, "SELECT COUNT(*) FROM properties as p JOIN charinfo as ci ON ci.prop_clone_id = p.clone_id where p.zone_id = ? AND (p.description LIKE ? OR p.name LIKE ? OR ci.name LIKE ?) AND p.privacy_option >= ? ", false);
	auto propertiesLeft = Database::CreatePreppedStmt(buttonQuery);

	propertiesLeft->setUInt(1, this->m_MapID);
	propertiesLeft->setString(2, searchString.c_str());
	propertiesLeft->setString(3, searchString.c_str());
	propertiesLeft->setString(4, searchString.c_str());
	propertiesLeft->setInt(5, sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS ? 1 : 2);

	auto result = propertiesLeft->executeQuery();
	result->next();
	numberOfProperties = result->getInt(1);

	delete result;
	result = nullptr;

	delete propertiesLeft;
	propertiesLeft = nullptr;

	GameMessages::SendPropertySelectQuery(m_Parent->GetObjectID(), startIndex, numberOfProperties - (startIndex + numResults) > 0, character->GetPropertyCloneID(), false, true, entries, sysAddr);
}
