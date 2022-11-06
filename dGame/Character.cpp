#include "Character.h"
#include "User.h"
#include "Database.h"
#include "GeneralUtils.h"
#include "dLogger.h"
#include "BitStream.h"
#include "Game.h"
#include <chrono>
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "dZoneManager.h"
#include "dServer.h"
#include "Zone.h"
#include "ChatPackets.h"
#include "Inventory.h"

Character::Character(uint32_t id, User* parentUser) {
	//First load the name, etc:
	m_ID = id;

	sql::PreparedStatement* stmt = Database::CreatePreppedStmt(
		"SELECT name, pending_name, needs_rename, prop_clone_id, permission_map FROM charinfo WHERE id=? LIMIT 1;"
	);

	stmt->setInt64(1, id);

	sql::ResultSet* res = stmt->executeQuery();

	while (res->next()) {
		m_Name = res->getString(1).c_str();
		m_UnapprovedName = res->getString(2).c_str();
		m_NameRejected = res->getBoolean(3);
		m_PropertyCloneID = res->getUInt(4);
		m_PermissionMap = static_cast<PermissionMap>(res->getUInt64(5));
	}

	delete res;
	delete stmt;

	//Load the xmlData now:
	sql::PreparedStatement* xmlStmt = Database::CreatePreppedStmt(
		"SELECT xml_data FROM charxml WHERE id=? LIMIT 1;"
	);

	xmlStmt->setInt64(1, id);

	sql::ResultSet* xmlRes = xmlStmt->executeQuery();
	while (xmlRes->next()) {
		m_XMLData = xmlRes->getString(1).c_str();
	}

	delete xmlRes;
	delete xmlStmt;

	m_ZoneID = 0; //TEMP! Set back to 0 when done. This is so we can see loading screen progress for testing.
	m_ZoneInstanceID = 0; //These values don't really matter, these are only used on the char select screen and seem unused.
	m_ZoneCloneID = 0;

	m_Doc = nullptr;

	//Quickly and dirtly parse the xmlData to get the info we need:
	DoQuickXMLDataParse();

	//Set our objectID:
	m_ObjectID = m_ID;
	m_ObjectID = GeneralUtils::SetBit(m_ObjectID, OBJECT_BIT_CHARACTER);
	m_ObjectID = GeneralUtils::SetBit(m_ObjectID, OBJECT_BIT_PERSISTENT);

	m_ParentUser = parentUser;
	m_OurEntity = nullptr;
	m_BuildMode = false;
}

Character::~Character() {
	delete m_Doc;
	m_Doc = nullptr;
}

void Character::UpdateFromDatabase() {
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt(
		"SELECT name, pending_name, needs_rename, prop_clone_id, permission_map FROM charinfo WHERE id=? LIMIT 1;"
	);

	stmt->setInt64(1, m_ID);

	sql::ResultSet* res = stmt->executeQuery();

	while (res->next()) {
		m_Name = res->getString(1).c_str();
		m_UnapprovedName = res->getString(2).c_str();
		m_NameRejected = res->getBoolean(3);
		m_PropertyCloneID = res->getUInt(4);
		m_PermissionMap = static_cast<PermissionMap>(res->getUInt64(5));
	}

	delete res;
	delete stmt;

	//Load the xmlData now:
	sql::PreparedStatement* xmlStmt = Database::CreatePreppedStmt(
		"SELECT xml_data FROM charxml WHERE id=? LIMIT 1;"
	);
	xmlStmt->setInt64(1, m_ID);

	sql::ResultSet* xmlRes = xmlStmt->executeQuery();
	while (xmlRes->next()) {
		m_XMLData = xmlRes->getString(1).c_str();
	}

	delete xmlRes;
	delete xmlStmt;

	m_ZoneID = 0; //TEMP! Set back to 0 when done. This is so we can see loading screen progress for testing.
	m_ZoneInstanceID = 0; //These values don't really matter, these are only used on the char select screen and seem unused.
	m_ZoneCloneID = 0;

	delete m_Doc;
	m_Doc = nullptr;

	//Quickly and dirtly parse the xmlData to get the info we need:
	DoQuickXMLDataParse();

	//Set our objectID:
	m_ObjectID = m_ID;
	m_ObjectID = GeneralUtils::SetBit(m_ObjectID, OBJECT_BIT_CHARACTER);
	m_ObjectID = GeneralUtils::SetBit(m_ObjectID, OBJECT_BIT_PERSISTENT);

	m_OurEntity = nullptr;
	m_BuildMode = false;
}

void Character::DoQuickXMLDataParse() {
	if (m_XMLData.size() == 0) return;

	delete m_Doc;
	m_Doc = new tinyxml2::XMLDocument();
	if (!m_Doc) return;

	if (m_Doc->Parse(m_XMLData.c_str(), m_XMLData.size()) == 0) {
		Game::logger->Log("Character", "Loaded xmlData for character %s (%i)!", m_Name.c_str(), m_ID);
	} else {
		Game::logger->Log("Character", "Failed to load xmlData!");
		//Server::rakServer->CloseConnection(m_ParentUser->GetSystemAddress(), true);
		return;
	}

	tinyxml2::XMLElement* mf = m_Doc->FirstChildElement("obj")->FirstChildElement("mf");
	if (!mf) {
		Game::logger->Log("Character", "Failed to find mf tag!");
		return;
	}

	mf->QueryAttribute("hc", &m_HairColor);
	mf->QueryAttribute("hs", &m_HairStyle);

	mf->QueryAttribute("t", &m_ShirtColor);
	mf->QueryAttribute("l", &m_PantsColor);

	mf->QueryAttribute("lh", &m_LeftHand);
	mf->QueryAttribute("rh", &m_RightHand);

	mf->QueryAttribute("es", &m_Eyebrows);
	mf->QueryAttribute("ess", &m_Eyes);
	mf->QueryAttribute("ms", &m_Mouth);

	tinyxml2::XMLElement* inv = m_Doc->FirstChildElement("obj")->FirstChildElement("inv");
	if (!inv) {
		Game::logger->Log("Character", "Char has no inv!");
		return;
	}

	tinyxml2::XMLElement* bag = inv->FirstChildElement("items")->FirstChildElement("in");

	if (!bag) {
		Game::logger->Log("Character", "Couldn't find bag0!");
		return;
	}

	while (bag != nullptr) {
		auto* sib = bag->FirstChildElement();

		while (sib != nullptr) {
			bool eq = false;
			sib->QueryAttribute("eq", &eq);
			LOT lot = 0;
			sib->QueryAttribute("l", &lot);

			if (eq) {
				if (lot != 0) m_EquippedItems.push_back(lot);
			}

			sib = sib->NextSiblingElement();
		}

		bag = bag->NextSiblingElement();
	}


	tinyxml2::XMLElement* character = m_Doc->FirstChildElement("obj")->FirstChildElement("char");
	if (character) {
		character->QueryAttribute("cc", &m_Coins);
		character->QueryAttribute("gm", &m_GMLevel);

		uint64_t lzidConcat = 0;
		if (character->FindAttribute("lzid")) {
			character->QueryAttribute("lzid", &lzidConcat);
			m_ZoneID = lzidConcat & ((1 << 16) - 1);
			m_ZoneInstanceID = (lzidConcat >> 16) & ((1 << 16) - 1);
			m_ZoneCloneID = (lzidConcat >> 32) & ((1 << 30) - 1);
		}

		//Darwin's backup
		if (character->FindAttribute("lwid")) {
			uint32_t worldID = 0;
			character->QueryAttribute("lwid", &worldID);
			m_ZoneID = worldID;
		}

		if (character->FindAttribute("lnzid")) {
			uint32_t lastNonInstanceZoneID = 0;
			character->QueryAttribute("lnzid", &lastNonInstanceZoneID);
			m_LastNonInstanceZoneID = lastNonInstanceZoneID;
		}

		if (character->FindAttribute("tscene")) {
			const char* tscene = nullptr;
			character->QueryStringAttribute("tscene", &tscene);
			m_TargetScene = std::string(tscene);
		}

		//To try and fix the AG landing into:
		if (m_ZoneID == 1000 && Game::server->GetZoneID() == 1100) {
			//sneakily insert our position:
			auto pos = dZoneManager::Instance()->GetZone()->GetSpawnPos();
			character->SetAttribute("lzx", pos.x);
			character->SetAttribute("lzy", pos.y);
			character->SetAttribute("lzz", pos.z);
		}

		auto emotes = character->FirstChildElement("ue");
		if (emotes) {
			auto currentChild = emotes->FirstChildElement();

			while (currentChild) {
				int emoteID;
				currentChild->QueryAttribute("id", &emoteID);
				m_UnlockedEmotes.push_back(emoteID);
				currentChild = currentChild->NextSiblingElement();
			}
		}

		character->QueryAttribute("lzx", &m_OriginalPosition.x);
		character->QueryAttribute("lzy", &m_OriginalPosition.y);
		character->QueryAttribute("lzz", &m_OriginalPosition.z);
		character->QueryAttribute("lzrx", &m_OriginalRotation.x);
		character->QueryAttribute("lzry", &m_OriginalRotation.y);
		character->QueryAttribute("lzrz", &m_OriginalRotation.z);
		character->QueryAttribute("lzrw", &m_OriginalRotation.w);
	}

	auto* flags = m_Doc->FirstChildElement("obj")->FirstChildElement("flag");
	if (flags) {
		auto* currentChild = flags->FirstChildElement();
		while (currentChild) {
			uint32_t index = 0;
			uint64_t value = 0;
			const auto* temp = currentChild->Attribute("v");

			index = std::stoul(currentChild->Attribute("id"));
			value = std::stoull(temp);

			m_PlayerFlags.insert(std::make_pair(index, value));
			currentChild = currentChild->NextSiblingElement();
		}
	}
}

void Character::UnlockEmote(int emoteID) {
	m_UnlockedEmotes.push_back(emoteID);
	GameMessages::SendSetEmoteLockState(EntityManager::Instance()->GetEntity(m_ObjectID), false, emoteID);
}

void Character::SetBuildMode(bool buildMode) {
	m_BuildMode = buildMode;

	auto* controller = dZoneManager::Instance()->GetZoneControlObject();

	controller->OnFireEventServerSide(m_OurEntity, buildMode ? "OnBuildModeEnter" : "OnBuildModeLeave");
}

void Character::SaveXMLToDatabase() {
	if (!m_Doc) return;

	//For metrics, we'll record the time it took to save:
	auto start = std::chrono::system_clock::now();

	tinyxml2::XMLElement* character = m_Doc->FirstChildElement("obj")->FirstChildElement("char");
	if (character) {
		character->SetAttribute("gm", m_GMLevel);
		character->SetAttribute("cc", m_Coins);

		auto zoneInfo = dZoneManager::Instance()->GetZone()->GetZoneID();
		// lzid garbage, binary concat of zoneID, zoneInstance and zoneClone
		if (zoneInfo.GetMapID() != 0 && zoneInfo.GetCloneID() == 0) {
			uint64_t lzidConcat = zoneInfo.GetCloneID();
			lzidConcat = (lzidConcat << 16) | uint16_t(zoneInfo.GetInstanceID());
			lzidConcat = (lzidConcat << 16) | uint16_t(zoneInfo.GetMapID());
			character->SetAttribute("lzid", lzidConcat);
			character->SetAttribute("lnzid", GetLastNonInstanceZoneID());

			//Darwin's backup:
			character->SetAttribute("lwid", Game::server->GetZoneID());

			// Set the target scene, custom attribute
			character->SetAttribute("tscene", m_TargetScene.c_str());
		}

		auto emotes = character->FirstChildElement("ue");
		if (!emotes) emotes = m_Doc->NewElement("ue");

		emotes->DeleteChildren();
		for (int emoteID : m_UnlockedEmotes) {
			auto emote = m_Doc->NewElement("e");
			emote->SetAttribute("id", emoteID);

			emotes->LinkEndChild(emote);
		}

		character->LinkEndChild(emotes);
	}

	//Export our flags:
	auto* flags = m_Doc->FirstChildElement("obj")->FirstChildElement("flag");
	if (!flags) {
		flags = m_Doc->NewElement("flag"); //Create a flags tag if we don't have one
		m_Doc->FirstChildElement("obj")->LinkEndChild(flags); //Link it to the obj tag so we can find next time
	}

	flags->DeleteChildren(); //Clear it if we have anything, so that we can fill it up again without dupes
	for (std::pair<uint32_t, uint64_t> flag : m_PlayerFlags) {
		auto* f = m_Doc->NewElement("f");
		f->SetAttribute("id", flag.first);

		//Because of the joy that is tinyxml2, it doesn't offer a function to set a uint64 as an attribute.
		//Only signed 64-bits ints would work.
		std::string v = std::to_string(flag.second);
		f->SetAttribute("v", v.c_str());

		flags->LinkEndChild(f);
	}

	SaveXmlRespawnCheckpoints();

	//Call upon the entity to update our xmlDoc:
	if (!m_OurEntity) {
		Game::logger->Log("Character", "We didn't have an entity set while saving! CHARACTER WILL NOT BE SAVED!");
		return;
	}

	m_OurEntity->UpdateXMLDoc(m_Doc);

	//Dump our xml into m_XMLData:
	auto* printer = new tinyxml2::XMLPrinter(0, true, 0);
	m_Doc->Print(printer);
	m_XMLData = printer->CStr();

	//Finally, save to db:
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("UPDATE charxml SET xml_data=? WHERE id=?");
	stmt->setString(1, m_XMLData.c_str());
	stmt->setUInt(2, m_ID);
	stmt->execute();
	delete stmt;

	//For metrics, log the time it took to save:
	auto end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed = end - start;
	Game::logger->Log("Character", "Saved character to Database in: %fs", elapsed.count());

	delete printer;
}

void Character::SetPlayerFlag(const uint32_t flagId, const bool value) {
	// If the flag is already set, we don't have to recalculate it
	if (GetPlayerFlag(flagId) == value) return;

	if (value) {
		// Update the mission component:
		auto* player = EntityManager::Instance()->GetEntity(m_ObjectID);

		if (player != nullptr) {
			auto* missionComponent = player->GetComponent<MissionComponent>();

			if (missionComponent != nullptr) {
				missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_PLAYER_FLAG, flagId);
			}
		}
	}

	// Calculate the index first
	auto flagIndex = uint32_t(std::floor(flagId / 64));

	const auto shiftedValue = 1ULL << flagId % 64;

	auto it = m_PlayerFlags.find(flagIndex);

	// Check if flag index exists
	if (it != m_PlayerFlags.end()) {
		// Update the value
		if (value) {
			it->second |= shiftedValue;
		} else {
			it->second &= ~shiftedValue;
		}
	} else {
		if (value) {
			// Otherwise, insert the value
			uint64_t flagValue = 0;

			flagValue |= shiftedValue;

			m_PlayerFlags.insert(std::make_pair(flagIndex, flagValue));
		}
	}

	// Notify the client that a flag has changed server-side
	GameMessages::SendNotifyClientFlagChange(m_ObjectID, flagId, value, m_ParentUser->GetSystemAddress());
}

bool Character::GetPlayerFlag(const uint32_t flagId) const {
	// Calculate the index first
	const auto flagIndex = uint32_t(std::floor(flagId / 64));

	const auto shiftedValue = 1ULL << flagId % 64;

	auto it = m_PlayerFlags.find(flagIndex);
	if (it != m_PlayerFlags.end()) {
		// Don't set the data if we don't have to
		return (it->second & shiftedValue) != 0;
	}

	return false; //by def, return false.
}

void Character::SetRetroactiveFlags() {
	// Retroactive check for if player has joined a faction to set their 'joined a faction' flag to true.
	if (GetPlayerFlag(ePlayerFlags::VENTURE_FACTION) || GetPlayerFlag(ePlayerFlags::ASSEMBLY_FACTION) || GetPlayerFlag(ePlayerFlags::PARADOX_FACTION) || GetPlayerFlag(ePlayerFlags::SENTINEL_FACTION)) {
		SetPlayerFlag(ePlayerFlags::JOINED_A_FACTION, true);
	}
}

void Character::SaveXmlRespawnCheckpoints() {
	//Export our respawn points:
	auto* points = m_Doc->FirstChildElement("obj")->FirstChildElement("res");
	if (!points) {
		points = m_Doc->NewElement("res");
		m_Doc->FirstChildElement("obj")->LinkEndChild(points);
	}

	points->DeleteChildren();
	for (const auto& point : m_WorldRespawnCheckpoints) {
		auto* r = m_Doc->NewElement("r");
		r->SetAttribute("w", point.first);

		r->SetAttribute("x", point.second.x);
		r->SetAttribute("y", point.second.y);
		r->SetAttribute("z", point.second.z);

		points->LinkEndChild(r);
	}
}

void Character::LoadXmlRespawnCheckpoints() {
	m_WorldRespawnCheckpoints.clear();

	auto* points = m_Doc->FirstChildElement("obj")->FirstChildElement("res");
	if (!points) {
		return;
	}

	auto* r = points->FirstChildElement("r");
	while (r != nullptr) {
		int32_t map = 0;
		NiPoint3 point = NiPoint3::ZERO;

		r->QueryAttribute("w", &map);
		r->QueryAttribute("x", &point.x);
		r->QueryAttribute("y", &point.y);
		r->QueryAttribute("z", &point.z);

		r = r->NextSiblingElement("r");

		m_WorldRespawnCheckpoints[map] = point;
	}

}

void Character::OnZoneLoad() {
	if (m_OurEntity == nullptr) {
		return;
	}

	auto* missionComponent = m_OurEntity->GetComponent<MissionComponent>();

	if (missionComponent != nullptr) {
		// Fix the monument race flag
		if (missionComponent->GetMissionState(319) >= MissionState::MISSION_STATE_READY_TO_COMPLETE) {
			SetPlayerFlag(33, true);
		}
	}

	const auto maxGMLevel = m_ParentUser->GetMaxGMLevel();

	// This does not apply to the GMs
	if (maxGMLevel > GAME_MASTER_LEVEL_CIVILIAN) {
		return;
	}

	/**
	 * Restrict old character to 1 million coins
	 */
	if (HasPermission(PermissionMap::Old)) {
		if (GetCoins() > 1000000) {
			SetCoins(1000000, eLootSourceType::LOOT_SOURCE_NONE);
		}
	}

	auto* inventoryComponent = m_OurEntity->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	// Remove all GM items
	for (const auto lot : Inventory::GetAllGMItems()) {
		inventoryComponent->RemoveItem(lot, inventoryComponent->GetLotCount(lot));
	}
}

PermissionMap Character::GetPermissionMap() const {
	return m_PermissionMap;
}

bool Character::HasPermission(PermissionMap permission) const {
	return (static_cast<uint64_t>(m_PermissionMap) & static_cast<uint64_t>(permission)) != 0;
}

void Character::SetRespawnPoint(LWOMAPID map, const NiPoint3& point) {
	m_WorldRespawnCheckpoints[map] = point;
}

const NiPoint3& Character::GetRespawnPoint(LWOMAPID map) const {
	const auto& pair = m_WorldRespawnCheckpoints.find(map);

	if (pair == m_WorldRespawnCheckpoints.end()) return NiPoint3::ZERO;

	return pair->second;
}

void Character::SetCoins(int64_t newCoins, eLootSourceType lootSource) {
	if (newCoins < 0) {
		newCoins = 0;
	}

	m_Coins = newCoins;

	GameMessages::SendSetCurrency(EntityManager::Instance()->GetEntity(m_ObjectID), m_Coins, 0, 0, 0, 0, true, lootSource);
}

bool Character::HasBeenToWorld(LWOMAPID mapID) const {
	return m_WorldRespawnCheckpoints.find(mapID) != m_WorldRespawnCheckpoints.end();
}

void Character::SendMuteNotice() const {
	if (!m_ParentUser->GetIsMuted()) return;

	time_t expire = m_ParentUser->GetMuteExpire();

	char buffer[32] = "brought up for review.\0";

	if (expire != 1) {
		std::tm* ptm = std::localtime(&expire);
		// Format: Mo, 15.06.2009 20:20:00
		std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm);
	}

	const auto timeStr = GeneralUtils::ASCIIToUTF16(std::string(buffer));

	ChatPackets::SendSystemMessage(GetEntity()->GetSystemAddress(), u"You are muted until " + timeStr);
}
