#include "PropertyEntranceComponent.h"

#include "CDPropertyEntranceComponentTable.h"

#include "Character.h"
#include "Database.h"
#include "GameMessages.h"
#include "PropertyManagementComponent.h"
#include "PropertyMessages.h"
#include "RocketLaunchpadControlComponent.h"
#include "CharacterComponent.h"
#include "UserManager.h"
#include "Logger.h"
#include "Amf3.h"
#include "eObjectBits.h"
#include "eGameMasterLevel.h"
#include "ePropertySortType.h"
#include "User.h"

PropertyEntranceComponent::PropertyEntranceComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
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

	GameMessages::PropertyEntranceBegin msg;
	msg.target = m_Parent->GetObjectID();
	msg.Send(entity->GetSystemAddress());

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

		cloneId = query[index].cloneId;
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
		playerEntry.ownerName = GeneralUtils::UTF8ToUTF16(character->GetName());
		playerEntry.isBff = true;
		playerEntry.isFriend = true;
		playerEntry.isAlt = true;
		playerEntry.isOwned = true;
		playerEntry.cloneId = playerProperty->cloneId;
		playerEntry.name = GeneralUtils::UTF8ToUTF16(playerProperty->name);
		playerEntry.description = GeneralUtils::UTF8ToUTF16(playerProperty->description);
		playerEntry.accessType = playerProperty->privacyOption;
		playerEntry.isModApproved = playerProperty->modApproved;
		playerEntry.dateLastPublished = playerProperty->lastUpdatedTime;
		playerEntry.reputation = playerProperty->reputation;
		playerEntry.performanceCost = playerProperty->performanceCost;
		auto& entry = playerEntry;
	} else {
		playerEntry.ownerName = GeneralUtils::UTF8ToUTF16(character->GetName());
		playerEntry.isBff = true;
		playerEntry.isFriend = true;
		playerEntry.isAlt = false;
		playerEntry.isOwned = false;
		playerEntry.cloneId = character->GetPropertyCloneID();
		playerEntry.name = u"";
		playerEntry.description = u"";
		playerEntry.accessType = 0;
		playerEntry.isModApproved = false;
		playerEntry.dateLastPublished = 0;
		playerEntry.reputation = 0;
		playerEntry.performanceCost = 0.0f;
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

	for (const auto& propertyEntry : lookupResult.entries) {
		const auto owner = propertyEntry.ownerId;
		const auto otherCharacter = Database::Get()->GetCharacterInfo(owner);
		if (!otherCharacter.has_value()) {
			LOG("Failed to find property owner name for %llu!", owner);
			continue;
		}
		auto& entry = entries.emplace_back();

		entry.isOwned = entry.cloneId == otherCharacter->cloneId;
		entry.ownerName = GeneralUtils::UTF8ToUTF16(otherCharacter->name);
		entry.cloneId = propertyEntry.cloneId;
		entry.name = GeneralUtils::UTF8ToUTF16(propertyEntry.name);
		entry.description = GeneralUtils::UTF8ToUTF16(propertyEntry.description);
		entry.accessType = propertyEntry.privacyOption;
		entry.isModApproved = propertyEntry.modApproved;
		entry.dateLastPublished = propertyEntry.lastUpdatedTime;
		entry.reputation = propertyEntry.reputation;
		entry.performanceCost = propertyEntry.performanceCost;
		entry.isBff = false;
		entry.isFriend = false;
		// Query to get friend and best friend fields
		const auto friendCheck = Database::Get()->GetBestFriendStatus(character->GetID(), owner);
		// If we got a result than the two players are friends.
		if (friendCheck.has_value()) {
			entry.isFriend = true;
			entry.isBff = friendCheck->bestFriendStatus == 3;
		}

		if (!entry.isModApproved && entity->GetGMLevel() >= eGameMasterLevel::LEAD_MODERATOR) {
			entry.name = u"[AWAITING APPROVAL]";
			entry.description = u"[AWAITING APPROVAL]";
			entry.isModApproved = true;
		}

		// Query to determine whether this property is an alt character of the entity.
		for (const auto charid : Database::Get()->GetAccountCharacterIds(user->GetAccountID())) {
			entry.isAlt = charid == owner;
			if (entry.isAlt) break;
		}
	}

	// Query here is to figure out whether or not to display the button to go to the next page or not.
	GameMessages::PropertySelectQuery query;
	query.target = m_Parent->GetObjectID();
	query.navOffset = startIndex;
	query.thereAreMore = lookupResult.totalEntriesMatchingQuery - (startIndex + numResults) > 0;
	query.cloneId = character->GetPropertyCloneID();
	query.hasFeaturedProperty = false;
	query.wasFriends = true;
	query.properties = entries;
	query.Send(sysAddr);
}
