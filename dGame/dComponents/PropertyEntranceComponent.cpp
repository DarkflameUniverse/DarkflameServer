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
#include "Logger.h"
#include "Amf3.h"
#include "eObjectBits.h"
#include "eGameMasterLevel.h"
#include "ePropertySortType.h"
#include "User.h"

PropertyEntranceComponent::PropertyEntranceComponent(Entity* parent, uint32_t componentID) : Component(parent) {
	this->propertyQueries = {};

	auto table = CDClientManager::GetTable<CDPropertyEntranceComponentTable>();
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

	args.Insert("state", "property_menu");

	GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "pushGameState", args);
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

void PropertyEntranceComponent::OnPropertyEntranceSync(Entity* entity, bool includeNullAddress, bool includeNullDescription, bool playerOwn, bool updateUi, int32_t numResults, int32_t lReputationTime, int32_t sortMethod, int32_t startIndex, std::string filterText, const SystemAddress& sysAddr) {
	const auto* const character = entity->GetCharacter();
	if (!character) return;
	const auto* const user = character->GetParentUser();
	if (!user) return;

	auto& entries = propertyQueries[entity->GetObjectID()];
	entries.clear();
	// Player property goes in index 1 of the vector.  This is how the client expects it.
	const auto playerProperty = Database::Get()->GetPropertyInfo(m_MapID, character->GetPropertyCloneID());

	// If the player has a property this query will have a single result.
	auto& playerEntry = entries.emplace_back();
	if (playerProperty.has_value()) {
		playerEntry.OwnerName = character->GetName();
		playerEntry.IsBestFriend = true;
		playerEntry.IsFriend = true;
		playerEntry.IsAlt = true;
		playerEntry.IsOwned = true;
		playerEntry.CloneId = playerProperty->cloneId;
		playerEntry.Name = playerProperty->name;
		playerEntry.Description = playerProperty->description;
		playerEntry.AccessType = playerProperty->privacyOption;
		playerEntry.IsModeratorApproved = playerProperty->modApproved;
		playerEntry.DateLastPublished = playerProperty->lastUpdatedTime;
		playerEntry.Reputation = playerProperty->reputation;
		playerEntry.PerformanceCost = playerProperty->performanceCost;
		auto& entry = playerEntry;
	} else {
		playerEntry.OwnerName = character->GetName();
		playerEntry.IsBestFriend = true;
		playerEntry.IsFriend = true;
		playerEntry.IsAlt = false;
		playerEntry.IsOwned = false;
		playerEntry.CloneId = character->GetPropertyCloneID();
		playerEntry.Name = "";
		playerEntry.Description = "";
		playerEntry.AccessType = 0;
		playerEntry.IsModeratorApproved = false;
		playerEntry.DateLastPublished = 0;
		playerEntry.Reputation = 0;
		playerEntry.PerformanceCost = 0.0f;
	}

	IProperty::PropertyLookup propertyLookup;
	propertyLookup.mapId = m_MapID;
	propertyLookup.searchString = filterText;
	propertyLookup.sortChoice = static_cast<ePropertySortType>(sortMethod);
	propertyLookup.playerSort = static_cast<uint32_t>(sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS ? PropertyPrivacyOption::Friends : PropertyPrivacyOption::Public);
	propertyLookup.playerId = character->GetID();
	propertyLookup.numResults = numResults;
	propertyLookup.startIndex = startIndex;

	const auto lookupResult = Database::Get()->GetProperties(propertyLookup);

	for (const auto& propertyEntry : lookupResult->entries) {
		const auto owner = propertyEntry.ownerId;
		const auto otherCharacter = Database::Get()->GetCharacterInfo(owner);
		if (!otherCharacter.has_value()) {
			LOG("Failed to find property owner name for %llu!", owner);
			continue;
		}
		auto& entry = entries.emplace_back();

		entry.IsOwned = entry.CloneId == otherCharacter->cloneId;
		entry.OwnerName = otherCharacter->name;
		entry.CloneId = propertyEntry.cloneId;
		entry.Name = propertyEntry.name;
		entry.Description = propertyEntry.description;
		entry.AccessType = propertyEntry.privacyOption;
		entry.IsModeratorApproved = propertyEntry.modApproved;
		entry.DateLastPublished = propertyEntry.lastUpdatedTime;
		entry.Reputation = propertyEntry.reputation;
		entry.PerformanceCost = propertyEntry.performanceCost;
		entry.IsBestFriend = false;
		entry.IsFriend = false;
		// Query to get friend and best friend fields
		const auto friendCheck = Database::Get()->GetBestFriendStatus(character->GetID(), owner);
		// If we got a result than the two players are friends.
		if (friendCheck.has_value()) {
			entry.IsFriend = true;
			entry.IsBestFriend = friendCheck->bestFriendStatus == 3;
		}

		if (!entry.IsModeratorApproved && entity->GetGMLevel() >= eGameMasterLevel::LEAD_MODERATOR) {
			entry.Name = "[AWAITING APPROVAL]";
			entry.Description = "[AWAITING APPROVAL]";
			entry.IsModeratorApproved = true;
		}

		// Query to determine whether this property is an alt character of the entity.
		for (const auto charid : Database::Get()->GetAccountCharacterIds(user->GetAccountID())) {
			entry.IsAlt = charid == owner;
			if (entry.IsAlt) break;
		}
	}

	// Query here is to figure out whether or not to display the button to go to the next page or not.
	GameMessages::SendPropertySelectQuery(m_Parent->GetObjectID(), startIndex, lookupResult->totalEntriesMatchingQuery - (startIndex + numResults) > 0, character->GetPropertyCloneID(), false, true, entries, sysAddr);
}
