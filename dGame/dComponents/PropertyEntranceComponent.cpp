#include "PropertyEntranceComponent.h"

#include <CDPropertyEntranceComponentTable.h>

#include "Character.h"
#include "Database.h"
#include "GameMessages.h"
#include "PropertyManagementComponent.h"
#include "PropertyData.h"
#include "RocketLaunchpadControlComponent.h"
#include "CharacterComponent.h"
#include "UserManager.h"
#include "Logger.h"
#include "Amf3.h"
#include "eObjectBits.h"
#include "eGameMasterLevel.h"
#include "User.h"

PropertyEntranceComponent::PropertyEntranceComponent(Entity* parent, uint32_t componentID) : Component(parent), m_UserRequestedCloneMap() {
	auto table = CDClientManager::Instance().GetTable<CDPropertyEntranceComponentTable>();
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

		const auto& item = m_UserRequestedCloneMap.find(entity->GetObjectID());
		if (item == m_UserRequestedCloneMap.end()) return;

		if (index >= m_UserRequestedCloneMap[entity->GetObjectID()].size()) return;

		cloneId = m_UserRequestedCloneMap[entity->GetObjectID()][index];
	}

	auto* launcher = m_Parent->GetComponent<RocketLaunchpadControlComponent>();

	if (launcher == nullptr) {
		return;
	}

	launcher->SetSelectedCloneId(entity->GetObjectID(), cloneId);

	launcher->Launch(entity, launcher->GetTargetZone(), cloneId);
}

void PropertyEntranceComponent::PopulateUserFriendMap(uint32_t user) {
	this->m_UserFriendMap[user] = {};

	auto friendQuery = Database::CreatePreppedStmt("SELECT * FROM friends WHERE friend_id = ? OR player_id = ?;");
	friendQuery->setUInt(1, user);
	friendQuery->setUInt(2, user);

	auto friendRes = friendQuery->executeQuery();

	while (friendRes->next()) {
		auto friendId = friendRes->getUInt(2);
		auto playerId = friendRes->getUInt(1);
		auto bestFriend = friendRes->getUInt(3) == 3;

		if (friendId == user) {
			this->m_UserFriendMap[user][playerId] = bestFriend;
		} else {
			this->m_UserFriendMap[user][friendId] = bestFriend;
		}
	}

	delete friendRes;
	friendRes = nullptr;

	delete friendQuery;
	friendQuery = nullptr;
}

std::vector<uint32_t> PropertyEntranceComponent::GetPropertyIDsBasedOnParams(const std::string& searchText, uint32_t sortMethod, Entity* requestor) {
	std::string query = "SELECT id, owner_id, last_updated, reputation FROM properties WHERE zone_id = ? AND (description LIKE ? OR name LIKE ? OR name LIKE ?) AND privacy_option >= ? ";

	if (sortMethod == SORT_TYPE_RECENT) {
		query += "ORDER BY last_updated DESC;";
	} else if (sortMethod == SORT_TYPE_REPUTATION) {
		query += "ORDER BY reputation DESC, last_updated DESC;";
	} else {
		query += "ORDER BY last_updated DESC;";
	}

	auto propertyLookup = Database::CreatePreppedStmt(query);

	const auto searchString = "%" + searchText + "%";
	propertyLookup->setUInt(1, this->m_MapID);
	propertyLookup->setString(2, searchString.c_str());
	propertyLookup->setString(3, searchString.c_str());
	propertyLookup->setString(4, searchString.c_str());
	propertyLookup->setInt(5, sortMethod == SORT_TYPE_FEATURED || sortMethod == SORT_TYPE_FRIENDS ? (uint32_t)PropertyPrivacyOption::Friends : (uint32_t)PropertyPrivacyOption::Public);

	auto propertyIdRes = propertyLookup->executeQuery();

	std::vector<uint32_t> propertyIds{};

	while (propertyIdRes->next()) {
		if (sortMethod == SORT_TYPE_FRIENDS || sortMethod == SORT_TYPE_FEATURED) {
			if (m_UserFriendMap[(uint32_t)requestor->GetObjectID()].find(propertyIdRes->getUInt(2)) == m_UserFriendMap[(uint32_t)requestor->GetObjectID()].end()) {
				continue;
			}
		}

		propertyIds.push_back(propertyIdRes->getUInt(1));
	}

	delete propertyIdRes;
	propertyIdRes = nullptr;

	delete propertyLookup;
	propertyLookup = nullptr;

	return propertyIds;
}


void PropertyEntranceComponent::OnPropertyEntranceSync(Entity* entity, bool includeNullAddress, bool includeNullDescription, bool playerOwn, bool updateUi, int32_t numResults, int32_t lReputationTime, int32_t sortMethod, int32_t startIndex, std::string filterText, const SystemAddress& sysAddr) {
	std::vector<PropertyData> entries{};

	if (m_UserFriendMap.find((uint32_t)entity->GetObjectID()) == m_UserFriendMap.end()) {
		this->PopulateUserFriendMap((uint32_t)entity->GetObjectID());
	}

	auto character = entity->GetCharacter();
	if (!character) return;

	auto playerEntry = this->GetPropertyData(character->GetPropertyCloneID());
	if (playerEntry.CloneID == 0) {
		playerEntry.CloneID = character->GetPropertyCloneID();
		playerEntry.PrimaryData.Name = character->GetName();

		playerEntry.PersonalData.IsFriend = true;
		playerEntry.PersonalData.IsBestFriend = true;
	} else {
		playerEntry.PersonalData = this->GetPropertyPersonalData(playerEntry, entity, true);
	}

	entries.push_back(playerEntry);

	auto propertyIds = this->GetPropertyIDsBasedOnParams(filterText, sortMethod, entity);

	std::vector<uint32_t> propertyIdsSlice(propertyIds.begin() + startIndex, propertyIds.begin() + std::min(startIndex + numResults, (int32_t)propertyIds.size()));

	for (const auto& id : propertyIdsSlice) {
		auto prop = this->GetPropertyData(id);
		if (prop.CloneID != 0) {
			prop.PersonalData = this->GetPropertyPersonalData(prop, entity, true);
			entries.push_back(prop);
		}
	}

	this->m_UserRequestedCloneMap[entity->GetObjectID()] = propertyIdsSlice;

	GameMessages::SendPropertySelectQuery(m_Parent->GetObjectID(), startIndex, propertyIds.size() - (startIndex + numResults) > 0, character->GetPropertyCloneID(), false, true, entries, sysAddr);
}


PropertyData PropertyEntranceComponent::GetPropertyData(uint32_t propertyID) {
	if (m_PropertyDataCache.find(propertyID) != m_PropertyDataCache.end()) {
		return m_PropertyDataCache[propertyID];
	}

	auto propQuery = Database::CreatePreppedStmt("SELECT * FROM properties WHERE id = ?;");
	propQuery->setUInt64(1, propertyID);

	auto result = propQuery->executeQuery();

	if (!result->next()) {
		delete result;
		result = nullptr;

		delete propQuery;
		propQuery = nullptr;

		return PropertyData{};
	}

	PropertyData entry{};

	entry.CloneID = result->getUInt64(4);

	auto charQuery = Database::CreatePreppedStmt("SELECT name FROM charinfo WHERE prop_clone_id = ?;");
	charQuery->setUInt64(1, entry.CloneID);

	auto charResult = charQuery->executeQuery();

	if (charResult->next()) {
		entry.PrimaryData.OwnerName = std::string(charResult->getString(1).c_str());
	}

	delete charResult;
	charResult = nullptr;

	delete charQuery;
	charQuery = nullptr;

	entry.PrimaryData.OwnerID = result->getInt(2);
	entry.PrimaryData.IsModeratorApproved = result->getBoolean(10);

	entry.PrimaryData.Name = std::string(result->getString(5).c_str());
	entry.PrimaryData.Description = std::string(result->getString(6).c_str());
	entry.PrimaryData.Reputation = result->getUInt(14);

	entry.MetaData.AccessType = result->getInt(9);
	entry.MetaData.DateLastPublished = result->getInt64(11);
	entry.MetaData.PerformanceCost = result->getFloat(16);

	delete result;
	result = nullptr;

	delete propQuery;
	propQuery = nullptr;

	this->m_PropertyDataCache[entry.CloneID] = entry;

	return entry;
}

PropertyPersonalData PropertyEntranceComponent::GetPropertyPersonalData(PropertyData& propertyData, Entity* queryingUser, bool updatePropertyDataStructure) {
	PropertyPersonalData personalData{};

	personalData.IsFriend = m_UserFriendMap[(uint32_t)queryingUser->GetObjectID()].find(propertyData.PrimaryData.OwnerID) != m_UserFriendMap[(uint32_t)queryingUser->GetObjectID()].end();
	if (personalData.IsFriend) {
		personalData.IsBestFriend = m_UserFriendMap[(uint32_t)queryingUser->GetObjectID()][propertyData.PrimaryData.OwnerID];
	}

	personalData.IsModeratorApproved = propertyData.PrimaryData.IsModeratorApproved;

	if (!personalData.IsModeratorApproved && queryingUser->GetGMLevel() >= eGameMasterLevel::LEAD_MODERATOR && updatePropertyDataStructure) {
		propertyData.PrimaryData.Name = "[AWAITING APPROVAL]";
		propertyData.PrimaryData.Description = "[AWAITING APPROVAL]";
		propertyData.PrimaryData.IsModeratorApproved = true;
		propertyData.PersonalData.IsModeratorApproved = true;
	}

	auto* user = UserManager::Instance()->GetUser(queryingUser->GetSystemAddress());

	auto isAltQuery = Database::CreatePreppedStmt("SELECT id, prop_clone_id FROM charinfo where account_id = ?;");

	isAltQuery->setInt(1, user->GetAccountID());

	auto isAltQueryResults = isAltQuery->executeQuery();

	bool ownerIsOurAccount = false;

	while (isAltQueryResults->next()) {
		if (queryingUser->GetCharacter()->GetPropertyCloneID() == isAltQueryResults->getInt(2)) {
			if (isAltQueryResults->getInt(1) == propertyData.PrimaryData.OwnerID) {
				personalData.IsOwned = true;
			}

			ownerIsOurAccount = true;
		}
	}

	personalData.IsAlt = ownerIsOurAccount && !personalData.IsOwned;

	delete isAltQueryResults;
	isAltQueryResults = nullptr;

	return personalData;
}
