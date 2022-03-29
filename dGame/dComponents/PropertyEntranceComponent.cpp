#include <CDPropertyEntranceComponentTable.h>
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
        // Increment index once here because the first index of other player properties is 2 in the propertyQueries cache.
        index++;

        const auto& pair = propertyQueries.find(entity->GetObjectID());

        if (pair == propertyQueries.end()) return;

        const auto& query = pair->second;

        if (index >= query.size()) return;

        cloneId = query[index].CloneId;
    }

    Game::logger->Log("PropertyEntranceComponent", "index is %i\n", index);

    auto* launcher = m_Parent->GetComponent<RocketLaunchpadControlComponent>();

    if (launcher == nullptr)
    {
        return;
    }

    launcher->SetSelectedCloneId(entity->GetObjectID(), cloneId);

    launcher->Launch(entity, LWOOBJID_EMPTY, launcher->GetTargetZone(), cloneId);
}

PropertySelectQueryProperty PropertyEntranceComponent::SetPropertyValues(PropertySelectQueryProperty property, LWOCLONEID cloneId, std::string ownerName, std::string propertyName, std::string propertyDescription, 
                                                                         uint32_t reputation, bool isBFF, bool isFriend, bool isModeratorApproved, bool isAlt, bool isOwned, uint32_t privacyOption, uint32_t timeLastUpdated, uint64_t performanceCost) {
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

std::string PropertyEntranceComponent::BuildQuery(Entity* entity, int32_t sortMethod) {
    auto base = baseQueryForProperties;
    std::string orderBy = "";
    std::string friendsList = " AND p.owner_id IN (";
    if (sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS) {
        auto friendsListQuery = Database::CreatePreppedStmt("SELECT * FROM (SELECT CASE WHEN player_id = ? THEN friend_id WHEN friend_id = ? THEN player_id END AS requested_player FROM dlu.friends ) AS fr WHERE requested_player IS NOT NULL ORDER BY requested_player DESC;");

        friendsListQuery->setInt64(1, entity->GetObjectID());
        friendsListQuery->setInt64(2, entity->GetObjectID());

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

        delete friendsListQueryResult;
        friendsListQueryResult = nullptr;
        delete friendsListQuery;
        friendsListQuery = nullptr;
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
    return baseQueryForProperties + orderBy + "LIMIT ? OFFSET ?;";
}

void PropertyEntranceComponent::OnPropertyEntranceSync(Entity* entity, bool includeNullAddress, bool includeNullDescription, bool playerOwn, bool updateUi, int32_t numResults, int32_t lReputationTime, int32_t sortMethod, int32_t startIndex, std::string filterText, const SystemAddress& sysAddr){

    std::vector<PropertySelectQueryProperty> entries {};
    PropertySelectQueryProperty playerEntry {};
    auto* character = entity->GetCharacter();
    if (!character) return;
    // Player property goes in index 1 of the vector.  This is how the client expects it.
    auto playerPropertyLookup = Database::CreatePreppedStmt("SELECT * FROM properties WHERE owner_id = ? AND zone_id = ?");

    playerPropertyLookup->setInt(1, character->GetID());
    playerPropertyLookup->setInt(2, this->m_MapID);

    auto playerPropertyLookupResults = playerPropertyLookup->executeQuery();

    // If the player has a property this query will have a single result.
    if (playerPropertyLookupResults->next()) {
        const auto cloneId = playerPropertyLookupResults->getUInt64(4);
        const auto name = playerPropertyLookupResults->getString(5).asStdString();
        const auto description = playerPropertyLookupResults->getString(6).asStdString();
        const auto privacyOption = playerPropertyLookupResults->getInt(9);
        const auto modApproved = playerPropertyLookupResults->getBoolean(10);
        const auto dateLastUpdated = playerPropertyLookupResults->getInt64(11);
        const auto reputation = playerPropertyLookupResults->getInt(14);

        playerEntry = SetPropertyValues(playerEntry, cloneId, character->GetName(), name, description, reputation, true, true, modApproved, true, true, privacyOption, dateLastUpdated);
    } else {
        playerEntry = SetPropertyValues(playerEntry, character->GetPropertyCloneID(), character->GetName(), "", "", 0, true, true);
    }

    delete playerPropertyLookupResults;
    playerPropertyLookupResults = nullptr;
    delete playerPropertyLookup;
    playerPropertyLookup = nullptr;

    entries.push_back(playerEntry);

    sql::ResultSet* propertyEntry;
    sql::PreparedStatement* propertyLookup;

    const auto query = BuildQuery(entity, sortMethod);

    propertyLookup = Database::CreatePreppedStmt(query);

    const std::string searchString = "%" + filterText + "%";
    propertyLookup->setUInt(1, this->m_MapID);
    propertyLookup->setString(2, searchString.c_str());
    propertyLookup->setString(3, searchString.c_str());
    propertyLookup->setString(4, searchString.c_str());
    propertyLookup->setInt(5, entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR || sortMethod == SORT_TYPE_FRIENDS ? 0 : 1);
    propertyLookup->setInt(6, sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS ? 1 : 2);
    propertyLookup->setInt(7, numResults);
    propertyLookup->setInt(8, startIndex);
    
    Game::logger->Log("PropertyEntranceComponent", "Property query is \n%s\n.  Entity is %s.\n", query.c_str(), entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR ? "a moderator" : "not a moderator");

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
        entry.DateLastPublished = dateUpdated;
        // Reputation not updated client side for listing?
        entry.Reputation = reputation;
        entry.CloneId = cloneId;
        entry.IsModeratorApproved = modApproved == true;
        entry.AccessType = privacyOption;
        // Client still reads performance cost as zero?
        entry.PerformanceCost = 0;

        entries.push_back(entry);
        delete nameLookup;
	}

    delete propertyLookup;

    propertyQueries[entity->GetObjectID()] = entries;

    GameMessages::SendPropertySelectQuery(m_Parent->GetObjectID(), startIndex, entries.size() >= numResults, character->GetPropertyCloneID(), false, true, entries, sysAddr);
}