#include "PropertyEntranceComponent.h"

#include "CDPropertyEntranceComponentTable.h"

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

PropertyEntranceComponent::PropertyEntranceComponent(Entity* parent, int32_t componentID) : Component(parent) {
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

		const auto& cachedQuery = propertyQueries.find(entity->GetObjectID());

		if (cachedQuery == propertyQueries.end()) {
			Game::logger->Log("PropertyEntranceComponent", "Player %llu:%s did not have a property query open", entity->GetObjectID(), entity->GetCharacter()->GetName().c_str());
			return;
		}

		const auto& query = cachedQuery->second;

		if (index >= query.size()) return;

		cloneId = query[index].CloneId;
	}

	auto* launcher = m_ParentEntity->GetComponent<RocketLaunchpadControlComponent>();

	if (!launcher) return;

	launcher->SetSelectedCloneId(entity->GetObjectID(), cloneId);

	launcher->Launch(entity, launcher->GetTargetZone(), cloneId);
}

std::string PropertyEntranceComponent::BuildQuery(const ePropertySortType sortMethod, Character* character, const std::string& customQuery, const bool wantLimits) {
	const std::string baseQueryForProperties =
		R"QUERY(
			SELECT p.* 
			FROM properties as p 
			JOIN charinfo as ci 
			ON ci.prop_clone_id = p.clone_id 
			WHERE p.zone_id = ? 
			AND (
				p.description LIKE ? 
				OR p.name LIKE ? 
				OR ci.name LIKE ? 
			) 
			AND p.privacy_option >= ? 
		)QUERY";
	DluAssert(character != nullptr);
	const std::string base = customQuery.empty() ? baseQueryForProperties : customQuery;
	std::string orderBy = "ORDER BY p.last_updated DESC";
	if (sortMethod == ePropertySortType::SORT_TYPE_FEATURED || sortMethod == ePropertySortType::SORT_TYPE_FRIENDS) {
		std::stringstream friendsStream;
		friendsStream << " AND p.owner_id IN (";

		std::unique_ptr<sql::PreparedStatement> friendsListQuery(Database::CreatePreppedStmt(
			"SELECT * FROM (SELECT CASE WHEN player_id = ? THEN friend_id WHEN friend_id = ? THEN player_id END AS requested_player FROM friends ) AS fr WHERE requested_player IS NOT NULL ORDER BY requested_player DESC;"
		));

		friendsListQuery->setUInt(1, character->GetID());
		friendsListQuery->setUInt(2, character->GetID());

		std::unique_ptr<sql::ResultSet> friendsListQueryResult(friendsListQuery->executeQuery());

		if (friendsListQueryResult->next()) {
			friendsStream << friendsListQueryResult->getInt("requested_player");
			while (friendsListQueryResult->next()) {
				friendsStream << ',' << friendsListQueryResult->getInt("requested_player");
			}
		}

		// If we have no friends then use a -1 for the query.
		if (friendsListQueryResult->rowsCount() == 0) friendsStream << -1;
		friendsStream << ')';

		orderBy += friendsStream.str() + " ORDER BY ci.name ASC";
	} else if (sortMethod == ePropertySortType::SORT_TYPE_RECENT) {
		orderBy = "ORDER BY p.last_updated DESC";
	} else if (sortMethod == ePropertySortType::SORT_TYPE_REPUTATION) {
		orderBy = "ORDER BY p.reputation DESC, p.last_updated DESC";
	}
	return base + orderBy + (wantLimits ? " LIMIT ? OFFSET ?;" : " ;");
}

void PropertyEntranceComponent::OnPropertyEntranceSync(Entity* entity, bool includeNullAddress, bool includeNullDescription, bool playerOwn, bool updateUi, int32_t numResults, int32_t lReputationTime, ePropertySortType sortMethod, int32_t startIndex, std::string filterText, const SystemAddress& sysAddr) {
	std::vector<PropertySelectQueryProperty> propertyPageEntries;
	PropertySelectQueryProperty requestorEntry;

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
	requestorEntry.CloneId = character->GetPropertyCloneID();
	requestorEntry.OwnerName = character->GetName();
	if (playerPropertyLookupResults->next()) {
		requestorEntry.Name = std::string(playerPropertyLookupResults->getString("name").c_str());
		requestorEntry.Description = std::string(playerPropertyLookupResults->getString("description").c_str());
		requestorEntry.Reputation = playerPropertyLookupResults->getUInt("reputation");
		requestorEntry.IsBestFriend = true;
		requestorEntry.IsFriend = true;
		requestorEntry.IsModeratorApproved = playerPropertyLookupResults->getBoolean("mod_approved");
		requestorEntry.IsAlt = true;
		requestorEntry.IsOwned = true;
		requestorEntry.AccessType = playerPropertyLookupResults->getInt("privacy_option");
		requestorEntry.DateLastPublished = playerPropertyLookupResults->getInt64("last_updated");
		requestorEntry.PerformanceCost = playerPropertyLookupResults->getDouble("performance_cost");
	}

	propertyPageEntries.push_back(requestorEntry);

	const auto query = BuildQuery(sortMethod, character);

	std::unique_ptr<sql::PreparedStatement> propertyLookup(Database::CreatePreppedStmt(query));

	const auto searchString = "%" + filterText + "%";
	PropertyPrivacyOption friendsLookup =
		sortMethod == ePropertySortType::SORT_TYPE_FEATURED || sortMethod == ePropertySortType::SORT_TYPE_FRIENDS ?
		PropertyPrivacyOption::Friends :
		PropertyPrivacyOption::Public;

	propertyLookup->setUInt(1, this->m_MapID);
	propertyLookup->setString(2, searchString.c_str());
	propertyLookup->setString(3, searchString.c_str());
	propertyLookup->setString(4, searchString.c_str());
	propertyLookup->setInt(5, static_cast<uint32_t>(friendsLookup));
	propertyLookup->setInt(6, numResults);
	propertyLookup->setInt(7, startIndex);

	std::unique_ptr<sql::ResultSet> propertyEntry(propertyLookup->executeQuery());

	while (propertyEntry->next()) {
		PropertySelectQueryProperty playerPropertyEntry;
		const auto owner = propertyEntry->getInt("owner_id");
		playerPropertyEntry.CloneId = propertyEntry->getUInt64("clone_id");
		playerPropertyEntry.Name = std::string(propertyEntry->getString("name").c_str());
		playerPropertyEntry.Description = std::string(propertyEntry->getString("description").c_str());
		playerPropertyEntry.AccessType = propertyEntry->getInt("privacy_option");
		playerPropertyEntry.IsModeratorApproved = propertyEntry->getBoolean("mod_approved");
		playerPropertyEntry.DateLastPublished = propertyEntry->getInt("last_updated");
		playerPropertyEntry.Reputation = static_cast<float>(propertyEntry->getInt("reputation"));
		playerPropertyEntry.PerformanceCost = static_cast<float>(propertyEntry->getDouble("performance_cost"));
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
		playerPropertyEntry.OwnerName = std::string(nameResult->getString("name").c_str());

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
		playerPropertyEntry.IsBestFriend = playerPropertyEntry.IsFriend && friendResult->getInt("best_friend") == 3;

		playerPropertyEntry.IsModeratorApproved = propertyEntry->getBoolean("mod_approved");

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

		propertyPageEntries.push_back(playerPropertyEntry);
	}
	propertyQueries[entity->GetObjectID()] = propertyPageEntries;

	// Query here is to figure out whether or not to display the button to go to the next page or not.
	const std::string baseButtonQuery =
		R"QUERY(
		SELECT COUNT(*) as numProperties 
		FROM properties as p 
		JOIN charinfo as ci 
		ON ci.prop_clone_id = p.clone_id 
		WHERE p.zone_id = ? 
		AND (
			p.description LIKE ? 
			OR p.name LIKE ? 
			OR ci.name LIKE ?
		) 
		AND p.privacy_option >= ? 
	)QUERY";

	auto buttonQuery = BuildQuery(sortMethod, character, baseButtonQuery, false);
	std::unique_ptr<sql::PreparedStatement> propertiesLeft(Database::CreatePreppedStmt(buttonQuery));

	propertiesLeft->setUInt(1, this->m_MapID);
	propertiesLeft->setString(2, searchString.c_str());
	propertiesLeft->setString(3, searchString.c_str());
	propertiesLeft->setString(4, searchString.c_str());
	propertiesLeft->setInt(5, static_cast<uint32_t>(friendsLookup));

	std::unique_ptr<sql::ResultSet> result(propertiesLeft->executeQuery());
	result->next();
	int32_t numberOfProperties = result->getInt("numProperties");

	GameMessages::SendPropertySelectQuery(m_ParentEntity->GetObjectID(), startIndex, numberOfProperties - (startIndex + numResults) > 0, character->GetPropertyCloneID(), false, true, propertyPageEntries, sysAddr);
}
