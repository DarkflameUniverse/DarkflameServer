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

void PropertyEntranceComponent::OnPropertyEntranceSync(Entity* entity,
                                                       bool includeNullAddress,
                                                       bool includeNullDescription,
                                                       bool playerOwn,
                                                       bool updateUi,
                                                       int32_t numResults,
                                                       int32_t reputation,
                                                       int32_t sortMethod,
                                                       int32_t startIndex,
                                                       std::string filterText,
                                                       const SystemAddress& sysAddr)
{
    Game::logger->Log("PropertyEntranceComponent", "On Sync %d %d %d %d %i %i %i %i %s\n",
        includeNullAddress,
        includeNullDescription,
        playerOwn,
        updateUi,
        numResults,
        reputation,
        sortMethod,
        startIndex,
        filterText.c_str()
    );

    auto* launchpadComponent = m_Parent->GetComponent<RocketLaunchpadControlComponent>();
    if (launchpadComponent == nullptr)
        return;

    std::vector<PropertySelectQueryProperty> entries {};
    PropertySelectQueryProperty playerEntry {};

    auto* character = entity->GetCharacter();
    playerEntry.OwnerName = character->GetName();
    playerEntry.Description = "No description.";
    playerEntry.Name = "Your property!";
    playerEntry.IsModeratorApproved = true;
    playerEntry.AccessType = 2;
    playerEntry.CloneId = character->GetPropertyCloneID();

    entries.push_back(playerEntry);

    sql::ResultSet* propertyEntry;
    sql::PreparedStatement* propertyLookup;
    std::string orderBy = "";
    int32_t privacyType = 2;

    const auto moderating = entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR;
    
    // We change how we sort this query based on what the requested order is.
	if (sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS) {
        orderBy = "ci.name";
        privacyType = 1;
    }
    else if (sortMethod == SORT_TYPE_RECENT) {
        orderBy = "p.last_updated";
    }
    else if (sortMethod == SORT_TYPE_REPUTATION) {
        orderBy = "p.reputation DESC, p.last_updated";
    }
    else {
        orderBy = "p.last_updated";
    }

    propertyLookup = Database::CreatePreppedStmt("SELECT p.* FROM properties as p JOIN charinfo as ci ON ci.prop_clone_id = p.clone_id where p.zone_id = ? AND (p.description OR p.name OR ci.name LIKE ?) AND p.mod_approved >= ? AND p.privacy_option = ? ORDER BY ? DESC LIMIT ? OFFSET ?;");

    const std::string searchString = "%" + filterText + "%";
    Game::logger->Log("PropertyEntranceComponent", "%s\n", searchString.c_str());
    propertyLookup->setUInt(1, launchpadComponent->GetTargetZone());
    propertyLookup->setString(2, searchString.c_str());
    propertyLookup->setInt(3, entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR ? 0 : 1);
    propertyLookup->setInt(4, privacyType);
    propertyLookup->setString(5, orderBy);
    propertyLookup->setInt(6, numResults);
    propertyLookup->setInt(7, startIndex);
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
		const auto reputation = propertyEntry->getInt(14);

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

        // Game::logger->Log("FriendsQuery", "Friend OBJID %lu my OBJID %lu\n", ownerObjId, entity->GetObjectID());
        auto isAltQuery = Database::CreatePreppedStmt("SELECT id FROM charinfo where account_id in (SELECT account_id from charinfo WHERE id = ?) AND id = ?;");

        isAltQuery->setInt(1, character->GetID());
        isAltQuery->setInt(2, owner);

        Game::logger->Log("PropertyEntranceComponent", "main character is %i with alt query being %i\n", character->GetID(), owner);

        auto isAltQueryResults = isAltQuery->executeQuery();

        if (isAltQueryResults->next()) {
            entry.IsAlt = true;
        } else {
            entry.IsAlt = false;
        }

        delete isAltQuery;
        isAltQuery = nullptr;

        entry.Reputation = reputation;
        entry.CloneId = cloneId;
        entry.IsModeratorApproved = modApproved == true;
        entry.AccessType = privacyOption;

        entries.push_back(entry);
        delete nameLookup;
	}

    delete propertyLookup;
    Game::logger->Log("HELLO", "WE GOT TO HERE");
    // auto newpropertyLookup = Database::CreatePreppedStmt("SELECT * FROM properties WHERE owner_id = ? AND zone_id = ?");

    // newpropertyLookup->setInt(1, character->GetID());
    // newpropertyLookup->setInt(2, launchpadComponent->GetTargetZone());

    // auto results = propertyLookup->executeQuery();

    // PropertySelectQueryProperty entry {};

    // entry.CloneId = character->GetPropertyCloneID();
    // entry.OwnerName = "";
    // entry.Name = "";
    // entry.Description = "";
    // entry.Reputation = 0;
    // entry.IsBestFriend = true;
    // entry.IsFriend = true;
    // entry.IsModeratorApproved = false;
    // entry.IsAlt = true;
    // entry.IsOwned = false;
    // entry.AccessType = 0;
    // // entry.DatePublished = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // // entry.DatePublished = 0;
    // // entry.PerformanceCost = entity->GetObjectID();

    // if (results->next()) {
    //     const auto propertyId = results->getUInt64(1);
	// 	const auto owner = results->getUInt64(2);
    //     const auto cloneId = results->getUInt64(4);
    //     const auto name = results->getString(5).asStdString();
    //     const auto description = results->getString(6).asStdString();
	// 	const auto privacyOption = results->getInt(9);
    //     const auto modApproved = results->getBoolean(10);
	// 	const auto reputation = results->getInt(14);

    //     // entry.CloneId = cloneId;
    //     // entry.OwnerName = character->GetName();
    //     // entry.Name = name;
    //     // entry.Description = description;
    //     // entry.Reputation = reputation;
    //     // entry.IsBestFriend = true;
    //     // entry.IsFriend = true;
    //     entry.IsModeratorApproved = modApproved;
    //     entry.IsAlt = true;
    //     entry.IsOwned = true;
    // }
    // entries.insert(entries.begin(), entry);
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