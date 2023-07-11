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
#include "Amf3.h"
#include "eObjectBits.h"
#include "eGameMasterLevel.h"
#include "DluAssert.h"

PropertyEntranceComponent::PropertyEntranceComponent(Entity* parent, uint32_t componentID) : Component(parent) {
	this->propertyQueries.clear();
	m_ComponentId = componentID;
}

void PropertyEntranceComponent::LoadTemplateData() {
	auto table = CDClientManager::Instance().GetTable<CDPropertyEntranceComponentTable>();
	const auto& entry = table->GetByID(m_ComponentId);

	m_MapID = entry.mapID;
	m_PropertyName = entry.propertyName;
}

void PropertyEntranceComponent::OnUse(Entity* entity) {
	auto* characterComponent = entity->GetComponent<CharacterComponent>();
	if (!characterComponent) return;

	auto* rocket = entity->GetComponent<CharacterComponent>()->RocketEquip(entity);
	if (!rocket) return;

	GameMessages::SendPropertyEntranceBegin(m_ParentEntity->GetObjectID(), entity->GetSystemAddress());

	AMFArrayValue args;

	args.Insert("state", "property_menu");

	GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "pushGameState", args);
}

void PropertyEntranceComponent::OnEnterProperty(Entity* entity, uint32_t index, bool returnToZone, const SystemAddress& sysAddr) {
	if (!entity->GetCharacter()) {
		Game::logger->Log("PropertyEntranceComponent", "Entity %llu attempted to enter a property with no character attached.", entity->GetObjectID());
	}
	LWOCLONEID cloneId = 0;

	if (index == -1 && !returnToZone) {
		cloneId = entity->GetCharacter()->GetPropertyCloneID();
	} else if (index >= 0) {
		// Increment index once here because the first index of other player properties is 2 in the propertyQueries cache.
		index++;

		const auto& pair = propertyQueries.find(entity->GetObjectID());

		if (pair == propertyQueries.end()) {
			Game::logger->Log("PropertyEntranceComponent", "Player %llu:%s did not have a property query open", entity->GetObjectID(), entity->GetCharacter()->GetName().c_str());
			return;
		}

		const auto& query = pair->second;

		if (index >= query.size()) return;

		cloneId = query[index].CloneId;
	}

	auto* launcher = m_ParentEntity->GetComponent<RocketLaunchpadControlComponent>();

	if (!launcher) return;

	launcher->SetSelectedCloneId(entity->GetObjectID(), cloneId);

	launcher->Launch(entity, launcher->GetTargetZone(), cloneId);
}

std::string PropertyEntranceComponent::BuildQuery(Entity* entity, int32_t sortMethod, Character* character, std::string customQuery, bool wantLimits) {
	DluAssert(character != nullptr);
	std::string base = customQuery.empty() ? baseQueryForProperties : customQuery;
	std::string orderBy;
	if (sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS) {
		std::stringstream friendsStream;
		friendsStream << " AND p.owner_id IN (";

		std::unique_ptr<sql::PreparedStatement> friendsListQuery(Database::CreatePreppedStmt(
			"SELECT * FROM (SELECT CASE WHEN player_id = ? THEN friend_id WHEN friend_id = ? THEN player_id END AS requested_player FROM friends ) AS fr WHERE requested_player IS NOT NULL ORDER BY requested_player DESC;"
		));

		friendsListQuery->setUInt(1, character->GetID());
		friendsListQuery->setUInt(2, character->GetID());

		std::unique_ptr<sql::ResultSet> friendsListQueryResult(friendsListQuery->executeQuery());

		if (friendsListQueryResult->next()) {
			friendsStream << friendsListQueryResult->getInt(1);
			while (friendsListQueryResult->next()) {
				friendsStream << ',' << friendsListQueryResult->getInt(1);
			}
		}
		// Replace trailing comma with the closing parenthesis.

		// If we have no friends then use a -1 for the query.
		if (friendsListQueryResult->rowsCount() == 0) friendsStream << -1;
		friendsStream << ')';

		orderBy += friendsStream.str() + " ORDER BY ci.name ASC";

	} else if (sortMethod == SORT_TYPE_RECENT) {
		orderBy = "ORDER BY p.last_updated DESC";
	} else if (sortMethod == SORT_TYPE_REPUTATION) {
		orderBy = "ORDER BY p.reputation DESC, p.last_updated DESC";
	} else {
		orderBy = "ORDER BY p.last_updated DESC";
	}
	return base + orderBy + (wantLimits ? " LIMIT ? OFFSET ?;" : " ;");
}

void PropertyEntranceComponent::OnPropertyEntranceSync(Entity* entity, bool includeNullAddress, bool includeNullDescription, bool playerOwn, bool updateUi, int32_t numResults, int32_t lReputationTime, int32_t sortMethod, int32_t startIndex, std::string filterText, const SystemAddress& sysAddr) {
	std::vector<PropertySelectQueryProperty> entries;
	PropertySelectQueryProperty playerEntry;

	auto character = entity->GetCharacter();
	if (!character) return;

	// The first index of every player requested query must be the players' own property since it is visible on every page.
	std::unique_ptr<sql::PreparedStatement> playerPropertyLookup(
		Database::CreatePreppedStmt(
			"SELECT * FROM properties WHERE owner_id = ? AND zone_id = ?"));

	playerPropertyLookup->setInt(1, character->GetID());
	playerPropertyLookup->setInt(2, m_MapID);

	std::unique_ptr<sql::ResultSet> playerPropertyLookupResults(playerPropertyLookup->executeQuery());

	// If the player has a property this query will have a single result.
	if (playerPropertyLookupResults->next()) {
		playerEntry.CloneId = playerPropertyLookupResults->getUInt64(4);;
		playerEntry.OwnerName = character->GetName();
		playerEntry.Name = playerPropertyLookupResults->getString(5).c_str();
		playerEntry.Description = playerPropertyLookupResults->getString(6).c_str();
		playerEntry.Reputation = playerPropertyLookupResults->getUInt(14);
		playerEntry.IsBestFriend = true;
		playerEntry.IsFriend = true;
		playerEntry.IsModeratorApproved = playerPropertyLookupResults->getBoolean(10);
		playerEntry.IsAlt = true;
		playerEntry.IsOwned = true;
		playerEntry.AccessType = playerPropertyLookupResults->getInt(9);
		playerEntry.DateLastPublished = playerPropertyLookupResults->getInt64(11);
		playerEntry.PerformanceCost = playerPropertyLookupResults->getDouble(16);
	} else {
		playerEntry.CloneId = character->GetPropertyCloneID();
		playerEntry.OwnerName = character->GetName();
		playerEntry.Description.clear();
		playerEntry.Name.clear();
		playerEntry.Reputation = 0.0f;
		playerEntry.IsBestFriend = true;
		playerEntry.IsFriend = true;
	}

	entries.push_back(playerEntry);

	const auto query = BuildQuery(entity, sortMethod, character);

	std::unique_ptr<sql::PreparedStatement> propertyLookup(Database::CreatePreppedStmt(query));

	const auto searchString = "%" + filterText + "%";
	propertyLookup->setUInt(1, this->m_MapID);
	propertyLookup->setString(2, searchString.c_str());
	propertyLookup->setString(3, searchString.c_str());
	propertyLookup->setString(4, searchString.c_str());
	propertyLookup->setInt(5, sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS ? (uint32_t)PropertyPrivacyOption::Friends : (uint32_t)PropertyPrivacyOption::Public);
	propertyLookup->setInt(6, numResults);
	propertyLookup->setInt(7, startIndex);

	std::unique_ptr<sql::ResultSet> propertyEntry(propertyLookup->executeQuery());

	while (propertyEntry->next()) {
		PropertySelectQueryProperty playerPropertyEntry;
		const auto owner = propertyEntry->getInt(2);
		playerPropertyEntry.CloneId = propertyEntry->getUInt64(4);
		playerPropertyEntry.Name = std::string(propertyEntry->getString(5).c_str());
		playerPropertyEntry.Description = std::string(propertyEntry->getString(6).c_str());
		playerPropertyEntry.AccessType = propertyEntry->getInt(9);
		playerPropertyEntry.IsModeratorApproved = propertyEntry->getBoolean(10);
		playerPropertyEntry.DateLastPublished = propertyEntry->getInt(11);
		playerPropertyEntry.Reputation = static_cast<float>(propertyEntry->getInt(14));
		playerPropertyEntry.PerformanceCost = static_cast<float>(propertyEntry->getDouble(16));
		playerPropertyEntry.OwnerName = "";
		playerPropertyEntry.IsBestFriend = false;
		playerPropertyEntry.IsFriend = false;
		playerPropertyEntry.IsAlt = false;
		playerPropertyEntry.IsOwned = true;
		std::unique_ptr<sql::PreparedStatement> nameLookup(
			Database::CreatePreppedStmt(
				"SELECT name FROM charinfo WHERE prop_clone_id = ?;"));

		nameLookup->setUInt64(1, playerPropertyEntry.CloneId);

		std::unique_ptr<sql::ResultSet> nameResult(nameLookup->executeQuery());

		if (!nameResult->next()) {
			Game::logger->Log("PropertyEntranceComponent", "Failed to find property owner name for %llu!", playerPropertyEntry.CloneId);
			continue;
		}
		playerPropertyEntry.IsOwned = playerPropertyEntry.CloneId == character->GetPropertyCloneID();
		playerPropertyEntry.OwnerName = std::string(nameResult->getString(1).c_str());

		// Query to get friend and best friend fields
		std::unique_ptr<sql::PreparedStatement> friendCheck(
			Database::CreatePreppedStmt(
				"SELECT best_friend FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?)"));

		friendCheck->setUInt(1, character->GetID());
		friendCheck->setUInt(2, owner);
		friendCheck->setUInt(3, owner);
		friendCheck->setUInt(4, character->GetID());

		std::unique_ptr<sql::ResultSet> friendResult(friendCheck->executeQuery());

		// If we got a result than the two players are friends.
		playerPropertyEntry.IsFriend = friendResult->next();
		playerPropertyEntry.IsBestFriend = playerPropertyEntry.IsFriend && friendResult->getInt(1) == 3;

		playerPropertyEntry.IsModeratorApproved = propertyEntry->getBoolean(10);

		// So lead moderators can visit the property but also see that it is still awaiting approval.
		if (!playerPropertyEntry.IsModeratorApproved && entity->GetGMLevel() >= eGameMasterLevel::LEAD_MODERATOR) {
			playerPropertyEntry.Name = "[AWAITING APPROVAL]";
			playerPropertyEntry.Description = "[AWAITING APPROVAL]";
			playerPropertyEntry.IsModeratorApproved = true;
		}

		// Query to determine whether this property is an alt character of the entity.
		std::unique_ptr<sql::PreparedStatement> isAltQuery(
			Database::CreatePreppedStmt(
				"SELECT id FROM charinfo where account_id in (SELECT account_id from charinfo WHERE id = ?) AND id = ?;"));

		isAltQuery->setInt(1, character->GetID());
		isAltQuery->setInt(2, owner);

		std::unique_ptr<sql::ResultSet> isAltQueryResults(isAltQuery->executeQuery());

		if (isAltQueryResults->next()) playerPropertyEntry.IsAlt = true;

		entries.push_back(playerPropertyEntry);
	}
	propertyQueries[entity->GetObjectID()] = entries;

	// Query here is to figure out whether or not to display the button to go to the next page or not.
	auto buttonQuery = BuildQuery(entity, sortMethod, character, "SELECT COUNT(*) as numProperties FROM properties as p JOIN charinfo as ci ON ci.prop_clone_id = p.clone_id where p.zone_id = ? AND (p.description LIKE ? OR p.name LIKE ? OR ci.name LIKE ?) AND p.privacy_option >= ? ", false);
	std::unique_ptr<sql::PreparedStatement> propertiesLeft(Database::CreatePreppedStmt(buttonQuery));

	propertiesLeft->setUInt(1, this->m_MapID);
	propertiesLeft->setString(2, searchString.c_str());
	propertiesLeft->setString(3, searchString.c_str());
	propertiesLeft->setString(4, searchString.c_str());
	propertiesLeft->setInt(5, sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS ? 1 : 2);

	std::unique_ptr<sql::ResultSet> result(propertiesLeft->executeQuery());
	result->next();
	int32_t numberOfProperties = result->getInt("numProperties");

	GameMessages::SendPropertySelectQuery(m_ParentEntity->GetObjectID(), startIndex, numberOfProperties - (startIndex + numResults) > 0, character->GetPropertyCloneID(), false, true, entries, sysAddr);
}
