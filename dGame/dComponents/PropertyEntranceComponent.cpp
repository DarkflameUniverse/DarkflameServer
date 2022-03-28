#include <CDPropertyEntranceComponentTable.h>
#include <chrono>
#include "PropertyEntranceComponent.h"
#include "PropertySelectQueryProperty.h"
#include "RocketLaunchpadControlComponent.h"
#include "Character.h"
#include "GameMessages.h"
#include "dLogger.h"
#include "Database.h"
#include "PropertyManagementComponent.h"
#include "UserManager.h"

PropertyEntranceComponent::PropertyEntranceComponent(uint32_t componentID, Entity* parent) : Component(parent)
{
    this->propertyQueries = {};

    auto table = CDClientManager::Instance()->GetTable<CDPropertyEntranceComponentTable>("PropertyEntranceComponent");
    const auto& entry = table->GetByID(componentID);

    this->m_MapID = entry.mapID;
    this->m_PropertyName = entry.propertyName;
}

void PropertyEntranceComponent::OnUse(Entity* entity)
{
    GameMessages::SendPropertyEntranceBegin(m_Parent->GetObjectID(), entity->GetSystemAddress());

    AMFArrayValue args;

	auto* state = new AMFStringValue();
	state->SetStringValue("property_menu");

	args.InsertValue("state", state);

    GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "pushGameState", &args);

    delete state;
}

void PropertyEntranceComponent::OnEnterProperty(Entity* entity, uint32_t index, bool returnToZone, const SystemAddress& sysAddr)
{
    LWOCLONEID cloneId = 0;

    if (index == -1 && !returnToZone)
    {
        cloneId = entity->GetCharacter()->GetPropertyCloneID();
    }
    else if (index == -1 && returnToZone)
    {
        cloneId = 0;
    }
    else if (index >= 0)
    {
        index++;

        const auto& pair = propertyQueries.find(entity->GetObjectID());

        if (pair == propertyQueries.end()) return;

        const auto& query = pair->second;

        if (index >= query.size()) return;

        cloneId = query[index].CloneId;
    }

    auto* launcher = m_Parent->GetComponent<RocketLaunchpadControlComponent>();

    if (launcher == nullptr)
    {
        return;
    }

    launcher->SetSelectedCloneId(entity->GetObjectID(), cloneId);

    launcher->Launch(entity, LWOOBJID_EMPTY, LWOMAPID_INVALID, cloneId);
}

void PropertyEntranceComponent::OnPropertyEntranceSync(Entity* entity, bool includeNullAddress, bool includeNullDescription, bool playerOwn, bool updateUi, int32_t numResults, int32_t lReputationTime, int32_t sortMethod, int32_t startIndex, std::string filterText, const SystemAddress& sysAddr){
    auto* launchpadComponent = m_Parent->GetComponent<RocketLaunchpadControlComponent>();
    if (launchpadComponent == nullptr) return;

    std::vector<PropertySelectQueryProperty> entries {};
    PropertySelectQueryProperty playerEntry {};
    auto* character = entity->GetCharacter();

    auto newpropertyLookup = Database::CreatePreppedStmt("SELECT * FROM properties WHERE owner_id = ? AND zone_id = ?");

    newpropertyLookup->setInt(1, character->GetID());
    newpropertyLookup->setInt(2, launchpadComponent->GetTargetZone());

    auto results = newpropertyLookup->executeQuery();

    playerEntry.CloneId = character->GetPropertyCloneID();
    playerEntry.OwnerName = "";
    playerEntry.Name = "";
    playerEntry.Description = "";
    playerEntry.Reputation = 0;
    playerEntry.IsBestFriend = true;
    playerEntry.IsFriend = true;
    playerEntry.IsModeratorApproved = false;
    playerEntry.IsAlt = true;
    playerEntry.IsOwned = false;
    playerEntry.AccessType = 0;
    playerEntry.DatePublished = 0;

    if (results->next()) {
        const auto propertyId = results->getUInt64(1);
        const auto owner = results->getUInt64(2);
        const auto cloneId = results->getUInt64(4);
        const auto name = results->getString(5).asStdString();
        const auto description = results->getString(6).asStdString();
        const auto privacyOption = results->getInt(9);
        const auto modApproved = results->getBoolean(10);
        const auto dateUpdated = results->getInt64(11);
        const auto reputation = results->getInt(14);

        playerEntry.Name = name;
        playerEntry.Description = description;
        playerEntry.DatePublished = dateUpdated;
        playerEntry.IsModeratorApproved = modApproved;
        playerEntry.IsAlt = true;
        playerEntry.IsOwned = true;
    }
    entries.push_back(playerEntry);

    sql::ResultSet* propertyEntry;
    sql::PreparedStatement* propertyLookup;
    std::string orderBy = "";
    int32_t privacyType = 2;

    const auto moderating = entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR;
    
    std::string baseQuery = "SELECT p.* FROM properties as p JOIN charinfo as ci ON ci.prop_clone_id = p.clone_id where p.zone_id = ? AND (p.description LIKE ? OR p.name LIKE ? OR ci.name LIKE ?) AND p.mod_approved >= ? AND p.privacy_option >= ? ";
    
    // In case the query is for friends.
    std::string friendsList = " AND p.owner_id IN (";
    // We change how we sort this query based on what the requested order is.
	if (sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS) {
        auto friendsListQuery = Database::CreatePreppedStmt("SELECT * FROM (SELECT CASE WHEN player_id = ? THEN friend_id WHEN friend_id = ? THEN player_id END AS requested_player FROM dlu.friends ) AS fr WHERE requested_player IS NOT NULL ORDER BY requested_player DESC;");

        friendsListQuery->setInt64(1, entity->GetObjectID());
        friendsListQuery->setInt64(2, entity->GetObjectID());
        // friendsListQuery->setInt(3, numResults);
        // friendsListQuery->setInt(4, startIndex);

        auto friendsListQueryResult = friendsListQuery->executeQuery();

        while (friendsListQueryResult->next()) {
            auto playerIDToConvert = friendsListQueryResult->getInt64(1);
            playerIDToConvert = GeneralUtils::ClearBit(playerIDToConvert, OBJECT_BIT_CHARACTER);
            playerIDToConvert = GeneralUtils::ClearBit(playerIDToConvert, OBJECT_BIT_PERSISTENT);
            friendsList = friendsList + std::to_string(playerIDToConvert) + ",";
        }
        // Replace trailing comma with the closing parenthesis.
        friendsList.replace(friendsList.size() - 1, 2, ") ");
        orderBy = friendsList + "ORDER BY ci.name ASC ";
        privacyType = 1;
    }
    else if (sortMethod == SORT_TYPE_RECENT) {
        orderBy = "ORDER BY p.last_updated DESC ";
    }
    else if (sortMethod == SORT_TYPE_REPUTATION) {
        orderBy = "ORDER BY p.reputation DESC, p.last_updated DESC ";
    }
    else {
        orderBy = "ORDER BY p.last_updated DESC ";
    }
    auto finishedQuery = baseQuery + orderBy + "LIMIT ? OFFSET ?;";
    propertyLookup = Database::CreatePreppedStmt(finishedQuery);

    const std::string searchString = "%" + filterText + "%";
    Game::logger->Log("PropertyEntranceComponent", "%s\n", searchString.c_str());
    propertyLookup->setUInt(1, launchpadComponent->GetTargetZone());
    propertyLookup->setString(2, searchString.c_str());
    propertyLookup->setString(3, searchString.c_str());
    propertyLookup->setString(4, searchString.c_str());
    propertyLookup->setInt(5, entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR || sortMethod == SORT_TYPE_FRIENDS ? 0 : 1);
    propertyLookup->setInt(6, privacyType);
    propertyLookup->setInt(7, numResults);
    propertyLookup->setInt(8, startIndex);
    Game::logger->Log("PropertyEntranceComponent", "Querying target zone %i with search string %s and ordering by %s starting at index %i. Entity is %s.\n", launchpadComponent->GetTargetZone(), searchString.c_str(), orderBy.c_str(), startIndex, entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR ? "a moderator" : "not a moderator");

    propertyEntry = propertyLookup->executeQuery();
	
	while (propertyEntry->next())
	{
		const auto propertyId = propertyEntry->getUInt64(1);
		const auto owner = propertyEntry->getInt(2);
        const auto cloneId = propertyEntry->getUInt64(4);
        const auto name = propertyEntry->getString(5).asStdString();
        const auto description = propertyEntry->getString(6).asStdString();
		const auto privacyOption = propertyEntry->getInt(9);
        const auto modApproved = propertyEntry->getBoolean(10);
        const auto dateUpdated = propertyEntry->getInt(11);
		const auto reputation = propertyEntry->getUInt(14);

        Game::logger->Log("PropertyEntranceComponent", "Property being loaded is %i with reputation %lu\n", owner, reputation);

        PropertySelectQueryProperty entry {};
        
	    auto* nameLookup = Database::CreatePreppedStmt("SELECT name FROM charinfo WHERE prop_clone_id = ?;");

        nameLookup->setUInt64(1, cloneId);

        auto* nameResult = nameLookup->executeQuery();

        if (!nameResult->next())
        {
            delete nameLookup;

            Game::logger->Log("PropertyEntranceComponent", "Failed to find property owner name for %llu!\n", cloneId);

            continue;
        }
        else
        {
            entry.IsOwned = owner == entity->GetCharacter()->GetID();
            entry.OwnerName = nameResult->getString(1).asStdString();
        }
		
        if (modApproved)
        {
            entry.Name = name;
            entry.Description = description;
        }

        // Convert owner char id to LWOOBJID
        LWOOBJID ownerObjId = owner;
        ownerObjId = GeneralUtils::SetBit(ownerObjId, OBJECT_BIT_CHARACTER);
        ownerObjId = GeneralUtils::SetBit(ownerObjId, OBJECT_BIT_PERSISTENT);

        auto friendCheck = Database::CreatePreppedStmt("SELECT best_friend FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?)");

        friendCheck->setInt64(1, entity->GetObjectID());
        friendCheck->setInt64(2, ownerObjId);
        friendCheck->setInt64(3, ownerObjId);
        friendCheck->setInt64(4, entity->GetObjectID());

        auto friendResult = friendCheck->executeQuery();
        
        // If we got a result than the two players are friends.
        if (friendResult->next()) {
            entry.IsFriend = true;
            if (friendResult->getBoolean(1) == true) {
                entry.IsBestFriend = true;
            } else {
                entry.IsBestFriend = false;
            }
        }
        else {
            entry.IsFriend = false;
            entry.IsBestFriend = false;
        }

        auto isAltQuery = Database::CreatePreppedStmt("SELECT id FROM charinfo where account_id in (SELECT account_id from charinfo WHERE id = ?) AND id = ?;");

        isAltQuery->setInt(1, character->GetID());
        isAltQuery->setInt(2, owner);

        auto isAltQueryResults = isAltQuery->executeQuery();

        if (isAltQueryResults->next()) {
            entry.IsAlt = true;
        } else {
            entry.IsAlt = false;
        }

        delete isAltQuery;
        isAltQuery = nullptr;
        entry.DatePublished = dateUpdated;
        entry.Reputation = reputation;
        entry.CloneId = cloneId;
        entry.IsModeratorApproved = modApproved == true;
        entry.AccessType = privacyOption;
        entry.PerformanceCost = entity->GetObjectID();

        entries.push_back(entry);
        delete nameLookup;
	}

    delete propertyLookup;

    propertyQueries[entity->GetObjectID()] = entries;

    GameMessages::SendPropertySelectQuery(
        m_Parent->GetObjectID(),
        startIndex,
        entries.size() >= numResults,
        character->GetPropertyCloneID(),
        false,
        true,
        entries,
        sysAddr
    );
}