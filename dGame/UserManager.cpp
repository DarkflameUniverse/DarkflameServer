#include "UserManager.h"
#include <fstream>
#include <future>
#include <sstream>
#include <algorithm>

#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "User.h"
#include <WorldPackets.h>
#include "Character.h"
#include <BitStream.h>
#include "PacketUtils.h"
#include "../dWorldServer/ObjectIDManager.h"
#include "Logger.h"
#include "GeneralUtils.h"
#include "ZoneInstanceManager.h"
#include "dServer.h"
#include "Entity.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "AssetManager.h"
#include "CDClientDatabase.h"
#include "eObjectBits.h"
#include "eGameMasterLevel.h"
#include "eCharacterCreationResponse.h"
#include "eRenameResponse.h"
#include "eConnectionType.h"
#include "eChatInternalMessageType.h"
#include "BitStreamUtils.h"
#include "CheatDetection.h"

UserManager* UserManager::m_Address = nullptr;

//Local functions as they aren't needed by anything else, leave the implementations at the bottom!
uint32_t FindCharShirtID(uint32_t shirtColor, uint32_t shirtStyle);
uint32_t FindCharPantsID(uint32_t pantsColor);

inline void StripCR(std::string& str) {
	str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
}

void UserManager::Initialize() {
	std::string line;

	AssetMemoryBuffer fnBuff = Game::assetManager->GetFileAsBuffer("names/minifigname_first.txt");
	if (!fnBuff.m_Success) {
		LOG("Failed to load %s", (Game::assetManager->GetResPath() / "names/minifigname_first.txt").string().c_str());
		throw std::runtime_error("Aborting initialization due to missing minifigure name file.");
	}
	std::istream fnStream = std::istream(&fnBuff);
	while (std::getline(fnStream, line, '\n')) {
		std::string name = line;
		StripCR(name);
		m_FirstNames.push_back(name);
	}
	fnBuff.close();

	AssetMemoryBuffer mnBuff = Game::assetManager->GetFileAsBuffer("names/minifigname_middle.txt");
	if (!mnBuff.m_Success) {
		LOG("Failed to load %s", (Game::assetManager->GetResPath() / "names/minifigname_middle.txt").string().c_str());
		throw std::runtime_error("Aborting initialization due to missing minifigure name file.");
	}
	std::istream mnStream = std::istream(&mnBuff);
	while (std::getline(mnStream, line, '\n')) {
		std::string name = line;
		StripCR(name);
		m_MiddleNames.push_back(name);
	}
	mnBuff.close();

	AssetMemoryBuffer lnBuff = Game::assetManager->GetFileAsBuffer("names/minifigname_last.txt");
	if (!lnBuff.m_Success) {
		LOG("Failed to load %s", (Game::assetManager->GetResPath() / "names/minifigname_last.txt").string().c_str());
		throw std::runtime_error("Aborting initialization due to missing minifigure name file.");
	}
	std::istream lnStream = std::istream(&lnBuff);
	while (std::getline(lnStream, line, '\n')) {
		std::string name = line;
		StripCR(name);
		m_LastNames.push_back(name);
	}
	lnBuff.close();

	//Load our pre-approved names:
	AssetMemoryBuffer chatListBuff = Game::assetManager->GetFileAsBuffer("chatplus_en_us.txt");
	if (!chatListBuff.m_Success) {
		LOG("Failed to load %s", (Game::assetManager->GetResPath() / "chatplus_en_us.txt").string().c_str());
		throw std::runtime_error("Aborting initialization due to missing chat whitelist file.");
	}
	std::istream chatListStream = std::istream(&chatListBuff);
	while (std::getline(chatListStream, line, '\n')) {
		StripCR(line);
		m_PreapprovedNames.push_back(line);
	}
	chatListBuff.close();
}

UserManager::~UserManager() {

}

User* UserManager::CreateUser(const SystemAddress& sysAddr, const std::string& username, const std::string& sessionKey) {
	User* user = new User(sysAddr, username, sessionKey);
	if (user && Game::server->IsConnected(sysAddr))
		m_Users.insert(std::make_pair(sysAddr, user));
	else {
		if (user) {
			delete user;
			user = nullptr;
		}
	}

	return user;
}

User* UserManager::GetUser(const SystemAddress& sysAddr) {
	auto it = m_Users.find(sysAddr);
	if (it != m_Users.end() && it->second) return it->second;

	return nullptr;
}

User* UserManager::GetUser(const std::string& username) {
	for (auto p : m_Users) {
		if (p.second) {
			if (GeneralUtils::CaseInsensitiveStringCompare(p.second->GetUsername(), username)) return p.second;
		}
	}

	return nullptr;
}

bool UserManager::DeleteUser(const SystemAddress& sysAddr) {
	const auto& it = m_Users.find(sysAddr);

	if (it != m_Users.end()) {
		if (std::count(m_UsersToDelete.begin(), m_UsersToDelete.end(), it->second)) return false;

		m_UsersToDelete.push_back(it->second);

		m_Users.erase(it);

		return true;
	}

	return false;
}

void UserManager::DeletePendingRemovals() {
	for (auto* user : m_UsersToDelete) {
		LOG("Deleted user %i", user->GetAccountID());

		delete user;
	}

	m_UsersToDelete.clear();
}

std::string UserManager::GetPredefinedName(uint32_t firstNameIndex, uint32_t middleNameIndex, uint32_t lastNameIndex) {
	if (firstNameIndex > m_FirstNames.size() || middleNameIndex > m_MiddleNames.size() || lastNameIndex > m_LastNames.size()) return std::string("INVALID");
	return std::string(m_FirstNames[firstNameIndex] + m_MiddleNames[middleNameIndex] + m_LastNames[lastNameIndex]);
}

bool UserManager::IsNamePreapproved(const std::string& requestedName) {
	for (std::string& s : m_PreapprovedNames) {
		if (s == requestedName) return true;
	}

	for (std::string& s : m_FirstNames) {
		if (s == requestedName) return true;
	}

	for (std::string& s : m_MiddleNames) {
		if (s == requestedName) return true;
	}

	for (std::string& s : m_LastNames) {
		if (s == requestedName) return true;
	}

	return false;
}

void UserManager::RequestCharacterList(const SystemAddress& sysAddr) {
	User* u = GetUser(sysAddr);
	if (!u) return;
	std::vector<Character*>& chars = u->GetCharacters();

	for (size_t i = 0; i < chars.size(); ++i) {
		if (chars[i]->GetEntity() == nullptr) // We don't have entity data to save
		{
			delete chars[i];

			continue;
		}

		auto* skillComponent = chars[i]->GetEntity()->GetComponent<SkillComponent>();

		if (skillComponent != nullptr) {
			skillComponent->Reset();
		}

		Game::entityManager->DestroyEntity(chars[i]->GetEntity());

		chars[i]->SaveXMLToDatabase();

		chars[i]->GetEntity()->SetCharacter(nullptr);

		delete chars[i];
	}

	chars.clear();

	for (const auto& characterId : Database::Get()->GetAccountCharacterIds(u->GetAccountID())) {
		Character* character = new Character(characterId, u);
		character->UpdateFromDatabase();
		character->SetIsNewLogin();
		chars.push_back(character);
	}

	WorldPackets::SendCharacterList(sysAddr, u);
}

void UserManager::CreateCharacter(const SystemAddress& sysAddr, Packet* packet) {
	User* u = GetUser(sysAddr);
	if (!u) return;

	std::string name = PacketUtils::ReadString(8, packet, true);

	uint32_t firstNameIndex = PacketUtils::ReadU32(74, packet);
	uint32_t middleNameIndex = PacketUtils::ReadU32(78, packet);
	uint32_t lastNameIndex = PacketUtils::ReadU32(82, packet);
	std::string predefinedName = GetPredefinedName(firstNameIndex, middleNameIndex, lastNameIndex);

	uint32_t shirtColor = PacketUtils::ReadU32(95, packet);
	uint32_t shirtStyle = PacketUtils::ReadU32(99, packet);
	uint32_t pantsColor = PacketUtils::ReadU32(103, packet);
	uint32_t hairStyle = PacketUtils::ReadU32(107, packet);
	uint32_t hairColor = PacketUtils::ReadU32(111, packet);
	uint32_t lh = PacketUtils::ReadU32(115, packet);
	uint32_t rh = PacketUtils::ReadU32(119, packet);
	uint32_t eyebrows = PacketUtils::ReadU32(123, packet);
	uint32_t eyes = PacketUtils::ReadU32(127, packet);
	uint32_t mouth = PacketUtils::ReadU32(131, packet);

	LOT shirtLOT = FindCharShirtID(shirtColor, shirtStyle);
	LOT pantsLOT = FindCharPantsID(pantsColor);

	if (!name.empty() && Database::Get()->GetCharacterInfo(name)) {
		LOG("AccountID: %i chose unavailable name: %s", u->GetAccountID(), name.c_str());
		WorldPackets::SendCharacterCreationResponse(sysAddr, eCharacterCreationResponse::CUSTOM_NAME_IN_USE);
		return;
	}

	if (Database::Get()->GetCharacterInfo(predefinedName)) {
		LOG("AccountID: %i chose unavailable predefined name: %s", u->GetAccountID(), predefinedName.c_str());
		WorldPackets::SendCharacterCreationResponse(sysAddr, eCharacterCreationResponse::PREDEFINED_NAME_IN_USE);
		return;
	}

	if (name.empty()) {
		LOG("AccountID: %i is creating a character with predefined name: %s", u->GetAccountID(), predefinedName.c_str());
	} else {
		LOG("AccountID: %i is creating a character with name: %s (temporary: %s)", u->GetAccountID(), name.c_str(), predefinedName.c_str());
	}

	//Now that the name is ok, we can get an objectID from Master:
	ObjectIDManager::Instance()->RequestPersistentID([=](uint32_t objectID) {
		if (Database::Get()->GetCharacterInfo(objectID)) {
			LOG("Character object id unavailable, check object_id_tracker!");
			WorldPackets::SendCharacterCreationResponse(sysAddr, eCharacterCreationResponse::OBJECT_ID_UNAVAILABLE);
			return;
		}

		std::stringstream xml;
		xml << "<obj v=\"1\">";

		xml << "<mf hc=\"" << hairColor << "\" hs=\"" << hairStyle << "\" hd=\"0\" t=\"" << shirtColor << "\" l=\"" << pantsColor;
		xml << "\" hdc=\"0\" cd=\"" << shirtStyle << "\" lh=\"" << lh << "\" rh=\"" << rh << "\" es=\"" << eyebrows << "\" ";
		xml << "ess=\"" << eyes << "\" ms=\"" << mouth << "\"/>";

		xml << "<char acct=\"" << u->GetAccountID() << "\" cc=\"0\" gm=\"0\" ft=\"0\" llog=\"" << time(NULL) << "\" ";
		xml << "ls=\"0\" lzx=\"-626.5847\" lzy=\"613.3515\" lzz=\"-28.6374\" lzrx=\"0.0\" lzry=\"0.7015\" lzrz=\"0.0\" lzrw=\"0.7126\" ";
		xml << "stt=\"0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;\"></char>";

		xml << "<dest hm=\"4\" hc=\"4\" im=\"0\" ic=\"0\" am=\"0\" ac=\"0\" d=\"0\"/>";

		xml << "<inv><bag><b t=\"0\" m=\"20\"/><b t=\"1\" m=\"40\"/><b t=\"2\" m=\"240\"/><b t=\"3\" m=\"240\"/><b t=\"14\" m=\"40\"/></bag><items><in t=\"0\">";

		LWOOBJID lwoidforshirt = ObjectIDManager::GenerateRandomObjectID();
		LWOOBJID lwoidforpants;

		do {
			lwoidforpants = ObjectIDManager::GenerateRandomObjectID();
		} while (lwoidforpants == lwoidforshirt); //Make sure we don't have the same ID for both shirt and pants

		GeneralUtils::SetBit(lwoidforshirt, eObjectBits::CHARACTER);
		GeneralUtils::SetBit(lwoidforshirt, eObjectBits::PERSISTENT);
		GeneralUtils::SetBit(lwoidforpants, eObjectBits::CHARACTER);
		GeneralUtils::SetBit(lwoidforpants, eObjectBits::PERSISTENT);

		xml << "<i l=\"" << shirtLOT << "\" id=\"" << lwoidforshirt << "\" s=\"0\" c=\"1\" eq=\"1\" b=\"1\"/>";
		xml << "<i l=\"" << pantsLOT << "\" id=\"" << lwoidforpants << "\" s=\"1\" c=\"1\" eq=\"1\" b=\"1\"/>";

		xml << "</in></items></inv><lvl l=\"1\" cv=\"1\" sb=\"500\"/><flag></flag></obj>";

		//Check to see if our name was pre-approved:
		bool nameOk = IsNamePreapproved(name);
		if (!nameOk && u->GetMaxGMLevel() > eGameMasterLevel::FORUM_MODERATOR) nameOk = true;

		if (name != "") {

			std::string_view nameToAssign = !name.empty() && nameOk ? name : predefinedName;
			std::string pendingName = !name.empty() && !nameOk ? name : "";

			ICharInfo::Info info;
			info.name = nameToAssign;
			info.pendingName = pendingName;
			info.id = objectID;
			info.accountId = u->GetAccountID();

			Database::Get()->InsertNewCharacter(info);

			//Now finally insert our character xml:
			Database::Get()->InsertCharacterXml(objectID, xml.str());

			WorldPackets::SendCharacterCreationResponse(sysAddr, eCharacterCreationResponse::SUCCESS);
			UserManager::RequestCharacterList(sysAddr);
		}
		});
}

void UserManager::DeleteCharacter(const SystemAddress& sysAddr, Packet* packet) {
	User* u = GetUser(sysAddr);
	if (!u) {
		LOG("Couldn't get user to delete character");
		return;
	}

	LWOOBJID objectID = PacketUtils::ReadS64(8, packet);
	uint32_t charID = static_cast<uint32_t>(objectID);

	LOG("Received char delete req for ID: %llu (%u)", objectID, charID);

	bool hasCharacter = CheatDetection::VerifyLwoobjidIsSender(
		objectID,
		sysAddr,
		CheckType::User,
		"User %i tried to delete a character that it does not own!",
		u->GetAccountID());

	if (!hasCharacter) {
		WorldPackets::SendCharacterDeleteResponse(sysAddr, false);
	} else {
		LOG("Deleting character %i", charID);
		Database::Get()->DeleteCharacter(charID);

		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::PLAYER_REMOVED_NOTIFICATION);
		bitStream.Write(objectID);
		Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);

		WorldPackets::SendCharacterDeleteResponse(sysAddr, true);
	}
}

void UserManager::RenameCharacter(const SystemAddress& sysAddr, Packet* packet) {
	User* u = GetUser(sysAddr);
	if (!u) {
		LOG("Couldn't get user to delete character");
		return;
	}

	LWOOBJID objectID = PacketUtils::ReadS64(8, packet);
	GeneralUtils::ClearBit(objectID, eObjectBits::CHARACTER);
	GeneralUtils::ClearBit(objectID, eObjectBits::PERSISTENT);

	uint32_t charID = static_cast<uint32_t>(objectID);
	LOG("Received char rename request for ID: %llu (%u)", objectID, charID);

	std::string newName = PacketUtils::ReadString(16, packet, true);

	Character* character = nullptr;

	//Check if this user has this character:
	bool ownsCharacter = CheatDetection::VerifyLwoobjidIsSender(
		objectID,
		sysAddr,
		CheckType::User,
		"User %i tried to rename a character that it does not own!",
		u->GetAccountID());

	std::find_if(u->GetCharacters().begin(), u->GetCharacters().end(), [&](Character* c) {
		if (c->GetID() == charID) {
			character = c;
			return true;
		}
		return false;
		});

	if (!ownsCharacter || !character) {
		WorldPackets::SendCharacterRenameResponse(sysAddr, eRenameResponse::UNKNOWN_ERROR);
	} else if (ownsCharacter && character) {
		if (newName == character->GetName()) {
			WorldPackets::SendCharacterRenameResponse(sysAddr, eRenameResponse::NAME_UNAVAILABLE);
			return;
		}

		if (Database::Get()->GetCharacterInfo(newName)) {
			if (IsNamePreapproved(newName)) {
				Database::Get()->SetCharacterName(charID, newName);
				LOG("Character %s now known as %s", character->GetName().c_str(), newName.c_str());
				WorldPackets::SendCharacterRenameResponse(sysAddr, eRenameResponse::SUCCESS);
				UserManager::RequestCharacterList(sysAddr);
			} else {
				Database::Get()->SetPendingCharacterName(charID, newName);
				LOG("Character %s has been renamed to %s and is pending approval by a moderator.", character->GetName().c_str(), newName.c_str());
				WorldPackets::SendCharacterRenameResponse(sysAddr, eRenameResponse::SUCCESS);
				UserManager::RequestCharacterList(sysAddr);
			}
		} else {
			WorldPackets::SendCharacterRenameResponse(sysAddr, eRenameResponse::NAME_IN_USE);
		}
	} else {
		LOG("Unknown error occurred when renaming character, either hasCharacter or character variable != true.");
		WorldPackets::SendCharacterRenameResponse(sysAddr, eRenameResponse::UNKNOWN_ERROR);
	}
}

void UserManager::LoginCharacter(const SystemAddress& sysAddr, uint32_t playerID) {
	User* u = GetUser(sysAddr);
	if (!u) {
		LOG("Couldn't get user to log in character");
		return;
	}

	Character* character = nullptr;
	bool hasCharacter = false;
	std::vector<Character*>& characters = u->GetCharacters();

	for (size_t i = 0; i < characters.size(); ++i) {
		if (characters[i]->GetID() == playerID) { hasCharacter = true; character = characters[i]; }
	}

	if (hasCharacter && character) {
		Database::Get()->UpdateLastLoggedInCharacter(playerID);

		uint32_t zoneID = character->GetZoneID();
		if (zoneID == LWOZONEID_INVALID) zoneID = 1000; //Send char to VE

		ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, zoneID, character->GetZoneClone(), false, [=](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
			LOG("Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", character->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);
			if (character) {
				character->SetZoneID(zoneID);
				character->SetZoneInstance(zoneInstance);
				character->SetZoneClone(zoneClone);
			}
			WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
			return;
			});
	} else {
		LOG("Unknown error occurred when logging in a character, either hasCharacter or character variable != true.");
	}
}

uint32_t FindCharShirtID(uint32_t shirtColor, uint32_t shirtStyle) {
	try {
		auto stmt = CDClientDatabase::CreatePreppedStmt(
			"select obj.id from Objects as obj JOIN (select * from ComponentsRegistry as cr JOIN ItemComponent as ic on ic.id = cr.component_id where cr.component_type == 11) as icc on icc.id = obj.id where lower(obj._internalNotes) == ? AND icc.color1 == ? AND icc.decal == ?"
		);
		stmt.bind(1, "character create shirt");
		stmt.bind(2, static_cast<int>(shirtColor));
		stmt.bind(3, static_cast<int>(shirtStyle));
		auto tableData = stmt.execQuery();
		auto shirtLOT = tableData.getIntField(0, 4069);
		tableData.finalize();
		return shirtLOT;
	} catch (const std::exception& ex) {
		LOG("Could not look up shirt %i %i: %s", shirtColor, shirtStyle, ex.what());
		// in case of no shirt found in CDServer, return problematic red vest.
		return 4069;
	}
}

uint32_t FindCharPantsID(uint32_t pantsColor) {
	try {
		auto stmt = CDClientDatabase::CreatePreppedStmt(
			"select obj.id from Objects as obj JOIN (select * from ComponentsRegistry as cr JOIN ItemComponent as ic on ic.id = cr.component_id where cr.component_type == 11) as icc on icc.id = obj.id where lower(obj._internalNotes) == ? AND icc.color1 == ?"
		);
		stmt.bind(1, "cc pants");
		stmt.bind(2, static_cast<int>(pantsColor));
		auto tableData = stmt.execQuery();
		auto pantsLOT = tableData.getIntField(0, 2508);
		tableData.finalize();
		return pantsLOT;
	} catch (const std::exception& ex) {
		LOG("Could not look up pants %i: %s", pantsColor, ex.what());
		// in case of no pants color found in CDServer, return red pants.
		return 2508;
	}
}

void UserManager::SaveAllActiveCharacters() {
	for (auto user : m_Users) {
		if (user.second) {
			auto character = user.second->GetLastUsedChar();
			if (character) character->SaveXMLToDatabase();
		}
	}
}
