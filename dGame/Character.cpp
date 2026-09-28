#include "Character.h"
#include "DashboardNotify.h"
#include "User.h"
#include "Database.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "BitStream.h"
#include "Game.h"
#include "dConfig.h"
#include <chrono>
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "MissionMessages.h"
#include "PlayerMessages.h"
#include "MissionComponent.h"
#include "dZoneManager.h"
#include "dServer.h"
#include "Zone.h"
#include "ChatPackets.h"
#include "Inventory.h"
#include "InventoryComponent.h"
#include "eMissionTaskType.h"
#include "eMissionState.h"
#include "eObjectBits.h"
#include "eGameMasterLevel.h"
#include "ePlayerFlag.h"
#include "CDPlayerFlagsTable.h"
#include "EconomyLedger.h"
#include "eServerDisconnectIdentifiers.h"

Character::Character(LWOOBJID id, User* parentUser) {
	//First load the name, etc:
	m_ID = id;
	m_ParentUser = parentUser;
	m_OurEntity = nullptr;
	m_GMLevel = eGameMasterLevel::CIVILIAN;
	m_PermissionMap = static_cast<ePermissionMap>(0);
}

Character::~Character() {
	m_OurEntity = nullptr;
	m_ParentUser = nullptr;
}

void Character::UpdateInfoFromDatabase() {
	auto charInfo = Database::Get()->GetCharacterInfo(m_ID);

	if (charInfo) {
		m_Name = charInfo->name;
		m_UnapprovedName = charInfo->pendingName;
		m_NameRejected = charInfo->needsRename;
		m_PropertyCloneID = charInfo->cloneId;
		m_PermissionMap = charInfo->permissionMap;
	}

	// Load the xmlData now. Loading takes the character over: saves from any server that loaded it before are refused.
	auto saved = Database::Get()->ClaimCharacterXml(m_ID);
	m_XMLData = saved ? std::move(saved->xml) : "";
	m_SaveGeneration = saved ? saved->generation : 0;
	m_SaveRefused = false;
	if (m_XMLData.empty()) {
		LOG("Character %s (%llu) has no xml data!", m_Name.c_str(), m_ID);
		return;
	}

	m_ZoneID = 0; //TEMP! Set back to 0 when done. This is so we can see loading screen progress for testing.
	m_ZoneInstanceID = 0; //These values don't really matter, these are only used on the char select screen and seem unused.
	m_ZoneCloneID = 0;

	//Quickly and dirtly parse the xmlData to get the info we need:
	DoQuickXMLDataParse();

	//Set our objectID:
	m_ObjectID = m_ID;
	GeneralUtils::SetBit(m_ObjectID, eObjectBits::CHARACTER);

	m_OurEntity = nullptr;
	m_BuildMode = false;
}

void Character::UpdateFromDatabase() {
	UpdateInfoFromDatabase();
}

void Character::DoQuickXMLDataParse() {
	if (m_XMLData.size() == 0) return;

	if (m_Doc.Parse(m_XMLData.c_str(), m_XMLData.size()) == 0) {
		LOG("Loaded xmlData for character %s (%llu)!", m_Name.c_str(), m_ID);
	} else {
		LOG("Failed to load xmlData (%i) (%s) (%s)!", m_Doc.ErrorID(), m_Doc.ErrorIDToName(m_Doc.ErrorID()), m_Doc.ErrorStr());
		//Server::rakServer->CloseConnection(m_ParentUser->GetSystemAddress(), true);
		return;
	}

	auto* const obj = m_Doc.FirstChildElement("obj");
	if (!obj) {
		LOG("Character %s (%llu) xml has no obj tag!", m_Name.c_str(), m_ID);
		return;
	}

	tinyxml2::XMLElement* mf = obj->FirstChildElement("mf");
	if (!mf) {
		LOG("Failed to find mf tag!");
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

	tinyxml2::XMLElement* inv = obj->FirstChildElement("inv");
	if (!inv) {
		LOG("Char has no inv!");
		return;
	}

	auto* const items = inv->FirstChildElement("items");
	tinyxml2::XMLElement* bag = items ? items->FirstChildElement("in") : nullptr;

	if (!bag) {
		LOG("Couldn't find bag0!");
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


	tinyxml2::XMLElement* character = GetXmlObjChild("char");
	if (character) {
		character->QueryAttribute("cc", &m_Coins);
		int32_t gm_level = 0;
		character->QueryAttribute("gm", &gm_level);
		m_GMLevel = static_cast<eGameMasterLevel>(gm_level);

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
			auto pos = Game::zoneManager->GetZone()->GetSpawnPos();
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

	auto* flags = GetXmlObjChild("flag");
	if (flags) {
		auto* currentChild = flags->FirstChildElement();
		while (currentChild) {
			const auto* temp = currentChild->Attribute("v");
			const auto* id = currentChild->Attribute("id");
			const auto* si = currentChild->Attribute("si");
			if (temp && id) {
				uint32_t index = 0;
				uint64_t value = 0;

				index = std::stoul(id);
				value = std::stoull(temp);

				m_PlayerFlags.insert(std::make_pair(index, value));
			} else if (si) {
				auto value = GeneralUtils::TryParse<uint32_t>(si);
				if (value) m_SessionFlags.insert(value.value());
			}
			currentChild = currentChild->NextSiblingElement();
		}
	}
}

void Character::UnlockEmote(int emoteID) {
	m_UnlockedEmotes.push_back(emoteID);
	auto* const entity = Game::entityManager->GetEntity(m_ObjectID);
	GameMessages::SetEmoteLockState lockState;
	lockState.target = entity->GetObjectID();
	lockState.bLock = false;
	lockState.emoteID = emoteID;
	lockState.SendToClient(entity->GetSystemAddress());
}

void Character::SetBuildMode(bool buildMode) {
	m_BuildMode = buildMode;

	auto* controller = Game::zoneManager->GetZoneControlObject();

	controller->OnFireEventServerSide(m_OurEntity, buildMode ? "OnBuildModeEnter" : "OnBuildModeLeave");
}

void Character::SaveXMLToDatabase() {
	// Check that we can actually _save_ before saving
	if (!m_OurEntity) {
		LOG("%llu:%s didn't have an entity set while saving! CHARACTER WILL NOT BE SAVED!", this->GetID(), this->GetName().c_str());
		return;
	}

	// Without an obj tag the xml never loaded, so there is nothing valid to write back.
	auto* const obj = m_Doc.FirstChildElement("obj");
	if (!obj) {
		LOG("%llu:%s has no loaded xml while saving! CHARACTER WILL NOT BE SAVED!", this->GetID(), this->GetName().c_str());
		return;
	}

	//For metrics, we'll record the time it took to save:
	auto start = std::chrono::system_clock::now();

	tinyxml2::XMLElement* character = GetXmlObjChild("char");
	if (character) {
		character->SetAttribute("gm", static_cast<uint32_t>(m_GMLevel));
		character->SetAttribute("cc", m_Coins);

		auto zoneInfo = Game::zoneManager->GetZone()->GetZoneID();
		// lzid garbage, binary concat of zoneID, zoneInstance and zoneClone
		if (SavesLocationInThisZone()) {
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
		if (!emotes) emotes = m_Doc.NewElement("ue");

		emotes->DeleteChildren();
		for (int emoteID : m_UnlockedEmotes) {
			auto emote = m_Doc.NewElement("e");
			emote->SetAttribute("id", emoteID);

			emotes->LinkEndChild(emote);
		}

		character->LinkEndChild(emotes);
	}

	//Export our flags:
	auto* flags = GetXmlObjChild("flag");
	if (!flags) {
		flags = m_Doc.NewElement("flag"); //Create a flags tag if we don't have one
		obj->LinkEndChild(flags); //Link it to the obj tag so we can find next time
	}

	flags->DeleteChildren(); //Clear it if we have anything, so that we can fill it up again without dupes
	for (const auto& [index, flagBucket] : m_PlayerFlags) {
		auto* f = flags->InsertNewChildElement("f");
		f->SetAttribute("id", index);
		f->SetAttribute("v", flagBucket);
	}

	for (const auto& sessionFlag : m_SessionFlags) {
		auto* s = flags->InsertNewChildElement("s");
		s->SetAttribute("si", sessionFlag);
	}

	SaveXmlRespawnCheckpoints();

	m_OurEntity->UpdateXMLDoc(m_Doc);

	WriteToDatabase();

	//For metrics, log the time it took to save:
	auto end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed = end - start;
	LOG("%llu:%s Saved character to Database in: %fs", this->GetID(), this->GetName().c_str(), elapsed.count());
}

void Character::SetIsNewLogin() {
	// If we dont have a flag element, then we cannot have a s element as a child of flag.
	auto* flags = GetXmlObjChild("flag");
	if (!flags) return;

	auto* currentChild = flags->FirstChildElement();
	while (currentChild) {
		auto* nextChild = currentChild->NextSiblingElement();
		if (currentChild->Attribute("si")) {
			LOG("Removed session flag (%s) from character %llu:%s, saving character to database", currentChild->Attribute("si"), GetID(), GetName().c_str());
			flags->DeleteChild(currentChild);
			WriteToDatabase();
		}
		currentChild = nextChild;
	}
}

void Character::WriteToDatabase() {
	//Dump our xml into m_XMLData:
	tinyxml2::XMLPrinter printer(0, true, 0);
	m_Doc.Print(&printer);

	// Update the xml on the character for future use if needed
	m_XMLData = printer.CStr();

	//Finally, save to db:
	if (m_SaveRefused) {
		LOG("Not saving character %llu:%s: newer data was saved elsewhere since it was loaded here", m_ID, m_Name.c_str());
		return;
	}
	if (!Database::Get()->SaveCharacterXml(m_ID, m_XMLData, m_SaveGeneration)) {
		OnStaleSave();
		return;
	}
	m_SaveGeneration++;
	DashboardNotify::Changed("characters", m_ID);
}

void Character::OnStaleSave() {
	m_SaveRefused = true;
	const auto stored = Database::Get()->GetCharacterSaveGeneration(m_ID);
	LOG("Refused a stale save of character %llu:%s: this server has save generation %llu, the database %llu (another world "
		"or the dashboard saved it since). The newer data is kept.", m_ID, m_Name.c_str(), m_SaveGeneration, stored);
	const auto accountId = m_ParentUser ? m_ParentUser->GetAccountID() : 0;
	const auto zone = Game::server ? Game::server->GetZoneID() : 0;
	const auto instance = Game::server ? Game::server->GetInstanceID() : 0;
	Database::Get()->InsertAuditLog(0, "World server", "stale_save_refused",
		m_Name + ": a save from zone " + std::to_string(zone) + " instance " + std::to_string(instance) + " (generation " + std::to_string(m_SaveGeneration) +
		") was refused because a newer one (generation " + std::to_string(stored) + ") is stored; the newer data was kept", accountId, m_ID);
	// If the player is still connected here, what they do next would be lost too: send them out so they load the
	// newer data. The disconnect is handled later, like any other.
	if (m_ParentUser && Game::server && Game::server->IsConnected(m_ParentUser->GetSystemAddress())) {
		Game::server->Disconnect(m_ParentUser->GetSystemAddress(), eServerDisconnectIdentifiers::SAVE_FAILURE);
	}
}

void Character::SetPlayerFlag(const uint32_t flagId, const bool value) {
	// If the flag is already set, we don't have to recalculate it
	if (GetPlayerFlag(flagId) == value) return;

	if (value) {
		// Update the mission component:
		auto* player = Game::entityManager->GetEntity(m_ObjectID);

		if (player != nullptr) {
			auto* missionComponent = player->GetComponent<MissionComponent>();

			if (missionComponent != nullptr) {
				missionComponent->Progress(eMissionTaskType::PLAYER_FLAG, flagId);
			}
		}
	}

	const auto flagEntry = CDPlayerFlagsTable::GetEntry(flagId);	

	if (flagEntry && flagEntry->sessionOnly) {
		if (value) m_SessionFlags.insert(flagId);
		else m_SessionFlags.erase(flagId);
	} else {
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
	}
	// Notify the client that a flag has changed server-side
	GameMessages::NotifyClientFlagChange flagChange;
	flagChange.target = m_ObjectID;
	flagChange.iFlagID = flagId;
	flagChange.bFlag = value;
	flagChange.SendToClient(m_ParentUser->GetSystemAddress());
}

bool Character::GetPlayerFlag(const uint32_t flagId) const {
	using enum ePlayerFlag;

	bool toReturn = false; //by def, return false.

	const auto flagEntry = CDPlayerFlagsTable::GetEntry(flagId);
	if (flagEntry && flagEntry->sessionOnly) {
		toReturn = m_SessionFlags.contains(flagId);
	} else {
		// Calculate the index first
		const auto flagIndex = uint32_t(std::floor(flagId / 64));

		const auto shiftedValue = 1ULL << flagId % 64;

		auto it = m_PlayerFlags.find(flagIndex);
		if (it != m_PlayerFlags.end()) {
			// Don't set the data if we don't have to
			toReturn = (it->second & shiftedValue) != 0;
		}
	}

	return toReturn;
}

void Character::SetRetroactiveFlags() {
	// Retroactive check for if player has joined a faction to set their 'joined a faction' flag to true.
	if (GetPlayerFlag(ePlayerFlag::VENTURE_FACTION) || GetPlayerFlag(ePlayerFlag::ASSEMBLY_FACTION) || GetPlayerFlag(ePlayerFlag::PARADOX_FACTION) || GetPlayerFlag(ePlayerFlag::SENTINEL_FACTION)) {
		SetPlayerFlag(ePlayerFlag::JOINED_A_FACTION, true);
	}
}

void Character::SaveXmlRespawnCheckpoints() {
	//Export our respawn points:
	auto* const obj = m_Doc.FirstChildElement("obj");
	if (!obj) return;

	auto* points = obj->FirstChildElement("res");
	if (!points) {
		points = m_Doc.NewElement("res");
		obj->LinkEndChild(points);
	}

	points->DeleteChildren();
	for (const auto& point : m_WorldRespawnCheckpoints) {
		auto* r = m_Doc.NewElement("r");
		r->SetAttribute("w", point.first);

		r->SetAttribute("x", point.second.x);
		r->SetAttribute("y", point.second.y);
		r->SetAttribute("z", point.second.z);

		points->LinkEndChild(r);
	}
}

void Character::LoadXmlRespawnCheckpoints() {
	m_WorldRespawnCheckpoints.clear();

	auto* points = GetXmlObjChild("res");
	if (!points) {
		return;
	}

	auto* r = points->FirstChildElement("r");
	while (r != nullptr) {
		int32_t map = 0;
		NiPoint3 point = NiPoint3Constant::ZERO;

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
		if (missionComponent->GetMissionState(319) >= eMissionState::READY_TO_COMPLETE) {
			SetPlayerFlag(ePlayerFlag::AG_FINISH_LINE_BUILT, true);
		}
	}

	const auto maxGMLevel = m_ParentUser->GetMaxGMLevel();

	// This does not apply to the GMs
	if (maxGMLevel > eGameMasterLevel::CIVILIAN) {
		return;
	}

	auto* inventoryComponent = m_OurEntity->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	// Remove all GM items
	for (const auto lot : Inventory::GetAllGMItems()) {
		inventoryComponent->RemoveItem(lot, inventoryComponent->GetLotCount(lot), eInventoryType::ALL);
	}
}

ePermissionMap Character::GetPermissionMap() const {
	return m_PermissionMap;
}

bool Character::HasPermission(ePermissionMap permission) const {
	return (static_cast<uint64_t>(m_PermissionMap) & static_cast<uint64_t>(permission)) != 0;
}

void Character::SetRespawnPoint(LWOMAPID map, const NiPoint3& point) {
	m_WorldRespawnCheckpoints[map] = point;
}

const NiPoint3& Character::GetRespawnPoint(LWOMAPID map) const {
	const auto& pair = m_WorldRespawnCheckpoints.find(map);

	if (pair == m_WorldRespawnCheckpoints.end()) return NiPoint3Constant::ZERO;

	return pair->second;
}

void Character::SetCoins(int64_t newCoins, eLootSourceType lootSource) {
	if (newCoins < 0) {
		newCoins = 0;
	}

	EconomyLedger::RecordCoins(m_ID, newCoins - m_Coins, lootSource);
	m_Coins = newCoins;

	auto* entity = Game::entityManager->GetEntity(m_ObjectID);
	GameMessages::SetCurrency setCurrency;
	setCurrency.target = entity->GetObjectID();
	setCurrency.currency = m_Coins;
	setCurrency.lootType = 0;
	setCurrency.sourceLOT = 0;
	setCurrency.sourceID = 0;
	setCurrency.sourceTradeID = 0;
	setCurrency.sourceType = lootSource;
	setCurrency.SendToClient(entity->GetSystemAddress());
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

void Character::SetBillboardVisible(bool visible) {
	if (m_BillboardVisible == visible) return;
	m_BillboardVisible = visible;

	GameMessages::SetNameBillboardState billboardState;
	billboardState.target = m_OurEntity->GetObjectID();
	billboardState.Send(UNASSIGNED_SYSTEM_ADDRESS);

	if (!visible) return;

	// The GameMessage we send for turning the nameplate off just deletes the BillboardSubcomponent from the parent component.
	// Because that same message does not allow for custom parameters, we need to create the BillboardSubcomponent a different way
	// This workaround involves sending an unrelated GameMessage that does not apply to player entites,
	// but forces the client to create the necessary SubComponent that controls the billboard.
	GameMessages::ShowBillboardInteractIcon interactIcon;
	interactIcon.target = m_OurEntity->GetObjectID();
	interactIcon.Send(UNASSIGNED_SYSTEM_ADDRESS);

	// Now turn off the billboard for the owner.
	GameMessages::SetNameBillboardState ownerBillboardState;
	ownerBillboardState.target = m_OurEntity->GetObjectID();
	ownerBillboardState.Send(m_OurEntity->GetSystemAddress());
}

bool Character::SavesLocationInThisZone() {
	const auto zoneInfo = Game::zoneManager->GetZone()->GetZoneID();
	if (zoneInfo.GetMapID() == 0 || Game::zoneManager->GetDisableSaveLocation()) return false;
	// By default logging in from a property goes to the last world before it
	return zoneInfo.GetCloneID() == 0 || (Game::config && Game::config->GetValue("save_property_location") == "1");
}
