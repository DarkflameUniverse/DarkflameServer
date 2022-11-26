#include "UserManager.h"
#include <fstream>
#include <future>
#include <sstream>
#include <algorithm>

#include "Database.h"
#include "Game.h"
#include "dLogger.h"
#include "User.h"
#include <WorldPackets.h>
#include "Character.h"
#include <BitStream.h>
#include "PacketUtils.h"
#include "../dWorldServer/ObjectIDManager.h"
#include "dLogger.h"
#include "GeneralUtils.h"
#include "ZoneInstanceManager.h"
#include "dServer.h"
#include "Entity.h"
#include "EntityManager.h"
#include "SkillComponent.h"

UserManager* UserManager::m_Address = nullptr;

//Local functions as they aren't needed by anything else, leave the implementations at the bottom!
uint32_t FindCharShirtID(uint32_t shirtColor, uint32_t shirtStyle);
uint32_t FindCharPantsID(uint32_t pantsColor);

inline void StripCR(std::string& str) {
	str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
}

void UserManager::Initialize() {
	std::string firstNamePath = "./res/names/minifigname_first.txt";
	std::string middleNamePath = "./res/names/minifigname_middle.txt";
	std::string lastNamePath = "./res/names/minifigname_last.txt";
	std::string line;

	std::fstream fnFile(firstNamePath, std::ios::in);
	std::fstream mnFile(middleNamePath, std::ios::in);
	std::fstream lnFile(lastNamePath, std::ios::in);

	while (std::getline(fnFile, line, '\n')) {
		std::string name = line;
		StripCR(name);
		m_FirstNames.push_back(name);
	}

	while (std::getline(mnFile, line, '\n')) {
		std::string name = line;
		StripCR(name);
		m_MiddleNames.push_back(name);
	}

	while (std::getline(lnFile, line, '\n')) {
		std::string name = line;
		StripCR(name);
		m_LastNames.push_back(name);
	}

	fnFile.close();
	mnFile.close();
	lnFile.close();

	//Load our pre-approved names:
	std::fstream chatList("./res/chatplus_en_us.txt", std::ios::in);
	while (std::getline(chatList, line, '\n')) {
		StripCR(line);
		m_PreapprovedNames.push_back(line);
	}
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
			if (p.second->GetUsername() == username) return p.second;
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
		Game::logger->Log("UserManager", "Deleted user %i", user->GetAccountID());

		delete user;
	}

	m_UsersToDelete.clear();
}

bool UserManager::IsNameAvailable(const std::string& requestedName) {
	bool toReturn = false; //To allow for a clean exit
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT id FROM charinfo WHERE name=? OR pending_name=? LIMIT 1;");
	stmt->setString(1, requestedName.c_str());
	stmt->setString(2, requestedName.c_str());

	sql::ResultSet* res = stmt->executeQuery();
	if (res->rowsCount() == 0) toReturn = true;

	delete stmt;
	delete res;
	return toReturn;
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

	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT id FROM charinfo WHERE account_id=? ORDER BY last_login DESC LIMIT 4;");
	stmt->setUInt(1, u->GetAccountID());

	sql::ResultSet* res = stmt->executeQuery();
	if (res->rowsCount() > 0) {
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

			EntityManager::Instance()->DestroyEntity(chars[i]->GetEntity());

			chars[i]->SaveXMLToDatabase();

			chars[i]->GetEntity()->SetCharacter(nullptr);

			delete chars[i];
		}

		chars.clear();

		while (res->next()) {
			LWOOBJID objID = res->getUInt64(1);
			Character* character = new Character(uint32_t(objID), u);
			chars.push_back(character);
		}
	}

	delete res;
	delete stmt;

	WorldPackets::SendCharacterList(sysAddr, u);
}

void UserManager::CreateCharacter(const SystemAddress& sysAddr, Packet* packet) {
	User* u = GetUser(sysAddr);
	if (!u) return;

	std::string name = PacketUtils::ReadString(8, packet, true);

	uint32_t firstNameIndex = PacketUtils::ReadPacketU32(74, packet);
	uint32_t middleNameIndex = PacketUtils::ReadPacketU32(78, packet);
	uint32_t lastNameIndex = PacketUtils::ReadPacketU32(82, packet);
	std::string predefinedName = GetPredefinedName(firstNameIndex, middleNameIndex, lastNameIndex);

	uint32_t shirtColor = PacketUtils::ReadPacketU32(95, packet);
	uint32_t shirtStyle = PacketUtils::ReadPacketU32(99, packet);
	uint32_t pantsColor = PacketUtils::ReadPacketU32(103, packet);
	uint32_t hairStyle = PacketUtils::ReadPacketU32(107, packet);
	uint32_t hairColor = PacketUtils::ReadPacketU32(111, packet);
	uint32_t lh = PacketUtils::ReadPacketU32(115, packet);
	uint32_t rh = PacketUtils::ReadPacketU32(119, packet);
	uint32_t eyebrows = PacketUtils::ReadPacketU32(123, packet);
	uint32_t eyes = PacketUtils::ReadPacketU32(127, packet);
	uint32_t mouth = PacketUtils::ReadPacketU32(131, packet);

	LOT shirtLOT = FindCharShirtID(shirtColor, shirtStyle);
	LOT pantsLOT = FindCharPantsID(pantsColor);

	if (name != "" && !UserManager::IsNameAvailable(name)) {
		Game::logger->Log("UserManager", "AccountID: %i chose unavailable name: %s", u->GetAccountID(), name.c_str());
		WorldPackets::SendCharacterCreationResponse(sysAddr, CREATION_RESPONSE_CUSTOM_NAME_IN_USE);
		return;
	}

	if (!IsNameAvailable(predefinedName)) {
		Game::logger->Log("UserManager", "AccountID: %i chose unavailable predefined name: %s", u->GetAccountID(), predefinedName.c_str());
		WorldPackets::SendCharacterCreationResponse(sysAddr, CREATION_RESPONSE_PREDEFINED_NAME_IN_USE);
		return;
	}

	if (name == "") {
		Game::logger->Log("UserManager", "AccountID: %i is creating a character with predefined name: %s", u->GetAccountID(), predefinedName.c_str());
	} else {
		Game::logger->Log("UserManager", "AccountID: %i is creating a character with name: %s (temporary: %s)", u->GetAccountID(), name.c_str(), predefinedName.c_str());
	}

	//Now that the name is ok, we can get an objectID from Master:
	ObjectIDManager::Instance()->RequestPersistentID([=](uint32_t objectID) {
		sql::PreparedStatement* overlapStmt = Database::CreatePreppedStmt("SELECT id FROM charinfo WHERE id = ?");
		overlapStmt->setUInt(1, objectID);

		auto* overlapResult = overlapStmt->executeQuery();

		if (overlapResult->next()) {
			Game::logger->Log("UserManager", "Character object id unavailable, check objectidtracker!");
			WorldPackets::SendCharacterCreationResponse(sysAddr, CREATION_RESPONSE_OBJECT_ID_UNAVAILABLE);
			return;
		}

		std::stringstream xml;
		xml << "<obj v=\"1\"><mf hc=\"" << hairColor << "\" hs=\"" << hairStyle << "\" hd=\"0\" t=\"" << shirtColor << "\" l=\"" << pantsColor;
		xml << "\" hdc=\"0\" cd=\"" << shirtStyle << "\" lh=\"" << lh << "\" rh=\"" << rh << "\" es=\"" << eyebrows << "\" ";
		xml << "ess=\"" << eyes << "\" ms=\"" << mouth << "\"/>";

		xml << "<char acct=\"" << u->GetAccountID() << "\" cc=\"0\" gm=\"0\" ft=\"0\" llog=\"" << time(NULL) << "\" ";
		xml << "ls=\"0\" lzx=\"-626.5847\" lzy=\"613.3515\" lzz=\"-28.6374\" lzrx=\"0.0\" lzry=\"0.7015\" lzrz=\"0.0\" lzrw=\"0.7126\" ";
		xml << "stt=\"0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;\"></char>";
		xml << "<dest hm=\"4\" hc=\"4\" im=\"0\" ic=\"0\" am=\"0\" ac=\"0\" d=\"0\"/>";
		xml << "<inv><bag><b t=\"0\" m=\"20\"/><b t=\"1\" m=\"40\"/><b t=\"2\" m=\"240\"/><b t=\"3\" m=\"240\"/><b t=\"14\" m=\"40\"/></bag><items><in t=\"0\">";
		std::string xmlSave1 = xml.str();

		ObjectIDManager::Instance()->RequestPersistentID([=](uint32_t idforshirt) {
			std::stringstream xml2;

			LWOOBJID lwoidforshirt = idforshirt;
			lwoidforshirt = GeneralUtils::SetBit(lwoidforshirt, OBJECT_BIT_CHARACTER);
			lwoidforshirt = GeneralUtils::SetBit(lwoidforshirt, OBJECT_BIT_PERSISTENT);
			xml2 << xmlSave1 << "<i l=\"" << shirtLOT << "\" id=\"" << lwoidforshirt << "\" s=\"0\" c=\"1\" eq=\"1\" b=\"1\"/>";

			std::string xmlSave2 = xml2.str();

			ObjectIDManager::Instance()->RequestPersistentID([=](uint32_t idforpants) {
				LWOOBJID lwoidforpants = idforpants;
				lwoidforpants = GeneralUtils::SetBit(lwoidforpants, OBJECT_BIT_CHARACTER);
				lwoidforpants = GeneralUtils::SetBit(lwoidforpants, OBJECT_BIT_PERSISTENT);

				std::stringstream xml3;
				xml3 << xmlSave2 << "<i l=\"" << pantsLOT << "\" id=\"" << lwoidforpants << "\" s=\"1\" c=\"1\" eq=\"1\" b=\"1\"/>";

				xml3 << "</in></items></inv><lvl l=\"1\" cv=\"1\" sb=\"500\"/><flag></flag></obj>";

				//Check to see if our name was pre-approved:
				bool nameOk = IsNamePreapproved(name);
				if (!nameOk && u->GetMaxGMLevel() > 1) nameOk = true;

				if (name != "") {
					sql::PreparedStatement* stmt = Database::CreatePreppedStmt("INSERT INTO `charinfo`(`id`, `account_id`, `name`, `pending_name`, `needs_rename`, `last_login`) VALUES (?,?,?,?,?,?)");
					stmt->setUInt(1, objectID);
					stmt->setUInt(2, u->GetAccountID());
					stmt->setString(3, predefinedName.c_str());
					stmt->setString(4, name.c_str());
					stmt->setBoolean(5, false);
					stmt->setUInt64(6, time(NULL));

					if (nameOk) {
						stmt->setString(3, name.c_str());
						stmt->setString(4, "");
					}

					stmt->execute();
					delete stmt;
				} else {
					sql::PreparedStatement* stmt = Database::CreatePreppedStmt("INSERT INTO `charinfo`(`id`, `account_id`, `name`, `pending_name`, `needs_rename`, `last_login`) VALUES (?,?,?,?,?,?)");
					stmt->setUInt(1, objectID);
					stmt->setUInt(2, u->GetAccountID());
					stmt->setString(3, predefinedName.c_str());
					stmt->setString(4, "");
					stmt->setBoolean(5, false);
					stmt->setUInt64(6, time(NULL));

					stmt->execute();
					delete stmt;
				}

				//Now finally insert our character xml:
				sql::PreparedStatement* stmt = Database::CreatePreppedStmt("INSERT INTO `charxml`(`id`, `xml_data`) VALUES (?,?)");
				stmt->setUInt(1, objectID);
				stmt->setString(2, xml3.str().c_str());
				stmt->execute();
				delete stmt;

				WorldPackets::SendCharacterCreationResponse(sysAddr, CREATION_RESPONSE_SUCCESS);
				UserManager::RequestCharacterList(sysAddr);
				});
			});
		});
}

void UserManager::DeleteCharacter(const SystemAddress& sysAddr, Packet* packet) {
	User* u = GetUser(sysAddr);
	if (!u) {
		Game::logger->Log("UserManager", "Couldn't get user to delete character");
		return;
	}

	LWOOBJID objectID = PacketUtils::ReadPacketS64(8, packet);
	uint32_t charID = static_cast<uint32_t>(objectID);

	Game::logger->Log("UserManager", "Received char delete req for ID: %llu (%u)", objectID, charID);

	//Check if this user has this character:
	bool hasCharacter = false;
	std::vector<Character*>& characters = u->GetCharacters();
	for (size_t i = 0; i < characters.size(); ++i) {
		if (characters[i]->GetID() == charID) { hasCharacter = true; }
	}

	if (!hasCharacter) {
		Game::logger->Log("UserManager", "User %i tried to delete a character that it does not own!", u->GetAccountID());
		WorldPackets::SendCharacterDeleteResponse(sysAddr, false);
	} else {
		Game::logger->Log("UserManager", "Deleting character %i", charID);
		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("DELETE FROM charxml WHERE id=? LIMIT 1;");
			stmt->setUInt64(1, charID);
			stmt->execute();
			delete stmt;
		}
		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("DELETE FROM command_log WHERE character_id=?;");
			stmt->setUInt64(1, charID);
			stmt->execute();
			delete stmt;
		}
		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("DELETE FROM friends WHERE player_id=? OR friend_id=?;");
			stmt->setUInt(1, charID);
			stmt->setUInt(2, charID);
			stmt->execute();
			delete stmt;
			CBITSTREAM;
			PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_PLAYER_REMOVED_NOTIFICATION);
			bitStream.Write(objectID);
			Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
		}
		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("DELETE FROM leaderboard WHERE character_id=?;");
			stmt->setUInt64(1, charID);
			stmt->execute();
			delete stmt;
		}
		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt(
				"DELETE FROM properties_contents WHERE property_id IN (SELECT id FROM properties WHERE owner_id=?);"
			);
			stmt->setUInt64(1, charID);
			stmt->execute();
			delete stmt;
		}
		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("DELETE FROM properties WHERE owner_id=?;");
			stmt->setUInt64(1, charID);
			stmt->execute();
			delete stmt;
		}
		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("DELETE FROM ugc WHERE character_id=?;");
			stmt->setUInt64(1, charID);
			stmt->execute();
			delete stmt;
		}
		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("DELETE FROM activity_log WHERE character_id=?;");
			stmt->setUInt64(1, charID);
			stmt->execute();
			delete stmt;
		}
		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("DELETE FROM mail WHERE receiver_id=?;");
			stmt->setUInt64(1, charID);
			stmt->execute();
			delete stmt;
		}
		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("DELETE FROM charinfo WHERE id=? LIMIT 1;");
			stmt->setUInt64(1, charID);
			stmt->execute();
			delete stmt;
		}

		WorldPackets::SendCharacterDeleteResponse(sysAddr, true);
	}
}

void UserManager::RenameCharacter(const SystemAddress& sysAddr, Packet* packet) {
	User* u = GetUser(sysAddr);
	if (!u) {
		Game::logger->Log("UserManager", "Couldn't get user to delete character");
		return;
	}

	LWOOBJID objectID = PacketUtils::ReadPacketS64(8, packet);
	objectID = GeneralUtils::ClearBit(objectID, OBJECT_BIT_CHARACTER);
	objectID = GeneralUtils::ClearBit(objectID, OBJECT_BIT_PERSISTENT);

	uint32_t charID = static_cast<uint32_t>(objectID);
	Game::logger->Log("UserManager", "Received char rename request for ID: %llu (%u)", objectID, charID);

	std::string newName = PacketUtils::ReadString(16, packet, true);

	Character* character = nullptr;

	//Check if this user has this character:
	bool hasCharacter = false;
	std::vector<Character*>& characters = u->GetCharacters();
	for (size_t i = 0; i < characters.size(); ++i) {
		if (characters[i]->GetID() == charID) { hasCharacter = true; character = characters[i]; }
	}

	if (!hasCharacter || !character) {
		Game::logger->Log("UserManager", "User %i tried to rename a character that it does not own!", u->GetAccountID());
		WorldPackets::SendCharacterRenameResponse(sysAddr, RENAME_RESPONSE_UNKNOWN_ERROR);
	} else if (hasCharacter && character) {
		if (newName == character->GetName()) {
			WorldPackets::SendCharacterRenameResponse(sysAddr, RENAME_RESPONSE_NAME_UNAVAILABLE);
			return;
		}

		if (IsNameAvailable(newName)) {
			if (IsNamePreapproved(newName)) {
				sql::PreparedStatement* stmt = Database::CreatePreppedStmt("UPDATE charinfo SET name=?, pending_name='', needs_rename=0, last_login=? WHERE id=? LIMIT 1");
				stmt->setString(1, newName);
				stmt->setUInt64(2, time(NULL));
				stmt->setUInt(3, character->GetID());
				stmt->execute();
				delete stmt;

				Game::logger->Log("UserManager", "Character %s now known as %s", character->GetName().c_str(), newName.c_str());
				WorldPackets::SendCharacterRenameResponse(sysAddr, RENAME_RESPONSE_SUCCESS);
				UserManager::RequestCharacterList(sysAddr);
			} else {
				sql::PreparedStatement* stmt = Database::CreatePreppedStmt("UPDATE charinfo SET pending_name=?, needs_rename=0, last_login=? WHERE id=? LIMIT 1");
				stmt->setString(1, newName);
				stmt->setUInt64(2, time(NULL));
				stmt->setUInt(3, character->GetID());
				stmt->execute();
				delete stmt;

				Game::logger->Log("UserManager", "Character %s has been renamed to %s and is pending approval by a moderator.", character->GetName().c_str(), newName.c_str());
				WorldPackets::SendCharacterRenameResponse(sysAddr, RENAME_RESPONSE_SUCCESS);
				UserManager::RequestCharacterList(sysAddr);
			}
		} else {
			WorldPackets::SendCharacterRenameResponse(sysAddr, RENAME_RESPONSE_NAME_IN_USE);
		}
	} else {
		Game::logger->Log("UserManager", "Unknown error occurred when renaming character, either hasCharacter or character variable != true.");
		WorldPackets::SendCharacterRenameResponse(sysAddr, RENAME_RESPONSE_UNKNOWN_ERROR);
	}
}

void UserManager::LoginCharacter(const SystemAddress& sysAddr, uint32_t playerID) {
	User* u = GetUser(sysAddr);
	if (!u) {
		Game::logger->Log("UserManager", "Couldn't get user to log in character");
		return;
	}

	Character* character = nullptr;
	bool hasCharacter = false;
	std::vector<Character*>& characters = u->GetCharacters();

	for (size_t i = 0; i < characters.size(); ++i) {
		if (characters[i]->GetID() == playerID) { hasCharacter = true; character = characters[i]; }
	}

	if (hasCharacter && character) {
		sql::PreparedStatement* stmt = Database::CreatePreppedStmt("UPDATE charinfo SET last_login=? WHERE id=? LIMIT 1");
		stmt->setUInt64(1, time(NULL));
		stmt->setUInt(2, playerID);
		stmt->execute();
		delete stmt;

		uint32_t zoneID = character->GetZoneID();
		if (zoneID == LWOZONEID_INVALID) zoneID = 1000; //Send char to VE

		ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, zoneID, character->GetZoneClone(), false, [=](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
			Game::logger->Log("UserManager", "Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", character->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);
			if (character) {
				character->SetZoneID(zoneID);
				character->SetZoneInstance(zoneInstance);
				character->SetZoneClone(zoneClone);
			}
			WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
			return;
			});
	} else {
		Game::logger->Log("UserManager", "Unknown error occurred when logging in a character, either hasCharacter or character variable != true.");
	}
}

uint32_t FindCharShirtID(uint32_t shirtColor, uint32_t shirtStyle) {
	try {
		std::string shirtQuery = "select obj.id from Objects as obj JOIN (select * from ComponentsRegistry as cr JOIN ItemComponent as ic on ic.id = cr.component_id where cr.component_type == 11) as icc on icc.id = obj.id where lower(obj._internalNotes) == \"character create shirt\" AND icc.color1 == ";
		shirtQuery += std::to_string(shirtColor);
		shirtQuery += " AND icc.decal == ";
		shirtQuery = shirtQuery + std::to_string(shirtStyle);
		auto tableData = CDClientDatabase::ExecuteQuery(shirtQuery);
		auto shirtLOT = tableData.getIntField(0, -1);
		tableData.finalize();
		return shirtLOT;
	} catch (const std::exception&) {
		Game::logger->Log("Character Create", "Failed to execute query! Using backup...");
		// in case of no shirt found in CDServer, return problematic red vest.
		return 4069;
	}
}

uint32_t FindCharPantsID(uint32_t pantsColor) {
	try {
		std::string pantsQuery = "select obj.id from Objects as obj JOIN (select * from ComponentsRegistry as cr JOIN ItemComponent as ic on ic.id = cr.component_id where cr.component_type == 11) as icc on icc.id = obj.id where lower(obj._internalNotes) == \"cc pants\" AND icc.color1 == ";
		pantsQuery += std::to_string(pantsColor);
		auto tableData = CDClientDatabase::ExecuteQuery(pantsQuery);
		auto pantsLOT = tableData.getIntField(0, -1);
		tableData.finalize();
		return pantsLOT;
	} catch (const std::exception&) {
		Game::logger->Log("Character Create", "Failed to execute query! Using backup...");
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
