#include <CDPropertyEntranceComponentTable.h>
#include "PropertyEntranceComponent.h"
#include "PropertySelectQueryProperty.h"
#include "RocketLaunchpadControlComponent.h"
#include "Character.h"
#include "GameMessages.h"
#include "dLogger.h"
#include "Database.h"
#include "PropertyManagementComponent.h"

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

    const auto moderating = entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR;
	
    if (!moderating)
    {
        propertyLookup = Database::CreatePreppedStmt(
                "SELECT * FROM properties WHERE (name LIKE ? OR description LIKE ? OR "
                "((SELECT name FROM charinfo WHERE prop_clone_id = clone_id) LIKE ?)) AND "
                "(privacy_option = 2 AND mod_approved = true) OR (privacy_option >= 1 "
                "AND (owner_id IN (SELECT friend_id FROM friends WHERE player_id = ?) OR owner_id IN (SELECT player_id FROM "
                "friends WHERE friend_id = ?))) AND zone_id = ? LIMIT ? OFFSET ?;"
                );

        const std::string searchString = "%" + filterText + "%";
        Game::logger->Log("PropertyEntranceComponent", "%s\n", searchString.c_str());
        propertyLookup->setString(1, searchString.c_str());
        propertyLookup->setString(2, searchString.c_str());
        propertyLookup->setString(3, searchString.c_str());
        propertyLookup->setInt64(4, entity->GetObjectID());
        propertyLookup->setInt64(5, entity->GetObjectID());
        propertyLookup->setUInt(6, launchpadComponent->GetTargetZone());
        propertyLookup->setInt(7, numResults);
        propertyLookup->setInt(8, startIndex);

        propertyEntry = propertyLookup->executeQuery();
    }
    else
    {
        propertyLookup = Database::CreatePreppedStmt(
            "SELECT * FROM properties WHERE privacy_option = 2 AND mod_approved = false AND zone_id = ?;"
        );

        propertyLookup->setUInt(1, launchpadComponent->GetTargetZone());

        propertyEntry = propertyLookup->executeQuery();
    }
	
	while (propertyEntry->next())
	{
		const auto propertyId = propertyEntry->getUInt64(1);
		const auto owner = propertyEntry->getUInt64(2);
        const auto cloneId = propertyEntry->getUInt64(4);
        const auto name = propertyEntry->getString(5).asStdString();
        const auto description = propertyEntry->getString(6).asStdString();
		const auto privacyOption = propertyEntry->getInt(9);
		const auto reputation = propertyEntry->getInt(15);

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
            entry.IsOwner = owner == entity->GetObjectID();
            entry.OwnerName = nameResult->getString(1).asStdString();
        }
		
        if (!moderating)
        {
            entry.Name = name;
            entry.Description = description;
        }
        else
        {
            entry.Name = "[Awaiting approval] " + name;
            entry.Description = "[Awaiting approval] " + description;
        }
		
        entry.IsFriend = privacyOption == static_cast<int32_t>(PropertyPrivacyOption::Friends);
        entry.Reputation = reputation;
        entry.CloneId = cloneId;
        entry.IsModeratorApproved = true;
        entry.AccessType = 3;

        entries.push_back(entry);

        delete nameLookup;
	}

    delete propertyLookup;

    /*
    const auto entriesSize = entries.size();

    if (startIndex != 0 && entriesSize > startIndex)
    {
        for (size_t i = 0; i < startIndex; i++)
        {
            entries.erase(entries.begin());
        }
    }
    */

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