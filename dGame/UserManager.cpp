#include "UserManager.h"
#include <fstream>
#include <future>
#include <sstream>
#include <algorithm>

#include "Database.h"
#include "Game.h"
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

UserManager * UserManager::m_Address = nullptr;

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
	
	//Load custom ones from MySQL too:
	/*sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT name FROM approvedNames;");
	sql::ResultSet* res = stmt->executeQuery();
	while (res->next()) {
		m_PreapprovedNames.push_back(res->getString(1));
	}
	
	delete res;
	delete stmt;*/
}

UserManager::~UserManager() {
	
}

User* UserManager::CreateUser ( const SystemAddress& sysAddr, const std::string& username, const std::string& sessionKey ) {
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

User* UserManager::GetUser ( const SystemAddress& sysAddr ) {
	auto it = m_Users.find(sysAddr);
	if (it != m_Users.end() && it->second) return it->second;

    return nullptr;
}

User* UserManager::GetUser ( const std::string& username ) {
	for (auto p : m_Users) {
		if (p.second) {
			if (p.second->GetUsername() == username) return p.second;
		}
	}

    return nullptr;
}

bool UserManager::DeleteUser ( const SystemAddress& sysAddr ) {
	const auto& it = m_Users.find(sysAddr);

	if (it != m_Users.end())
	{
		if (std::count(m_UsersToDelete.begin(), m_UsersToDelete.end(), it->second)) return false;

		m_UsersToDelete.push_back(it->second);
		
		m_Users.erase(it);

		return true;
	}
	
	return false;
}

void UserManager::DeletePendingRemovals() 
{
	for (auto* user : m_UsersToDelete)
	{
		Game::logger->Log("UserManager", "Deleted user %i\n", user->GetAccountID());

		delete user;
	}

	m_UsersToDelete.clear();
}

bool UserManager::IsNameAvailable ( const std::string& requestedName ) {
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

std::string UserManager::GetPredefinedName ( uint32_t firstNameIndex, uint32_t middleNameIndex, uint32_t lastNameIndex ) {
	if (firstNameIndex > m_FirstNames.size() || middleNameIndex > m_MiddleNames.size() || lastNameIndex > m_LastNames.size()) return std::string("INVALID");
	return std::string(m_FirstNames[firstNameIndex] + m_MiddleNames[middleNameIndex] + m_LastNames[lastNameIndex]);
}

bool UserManager::IsNamePreapproved ( const std::string& requestedName ) {
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

void UserManager::RequestCharacterList ( const SystemAddress& sysAddr ) {
	User* u = GetUser(sysAddr);
	if (!u) return;
	
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT id FROM charinfo WHERE account_id=? ORDER BY last_login DESC LIMIT 4;");
	stmt->setUInt(1, u->GetAccountID());
	
	sql::ResultSet* res = stmt->executeQuery();
	if (res->rowsCount() > 0) {
		std::vector<Character*>& chars = u->GetCharacters();
		
		for (size_t i = 0; i < chars.size(); ++i)
		{
			if (chars[i]->GetEntity() == nullptr) // We don't have entity data to save
			{
				delete chars[i];

				continue;
			}

			auto* skillComponent = chars[i]->GetEntity()->GetComponent<SkillComponent>();

			if (skillComponent != nullptr)
			{
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
    Game::logger->Log("UserManager", "Got predefined name: %s\n", predefinedName.c_str());

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
        WorldPackets::SendCharacterCreationResponse(sysAddr, CREATION_RESPONSE_CUSTOM_NAME_IN_USE);
        return;
    }
    
    if (!IsNameAvailable(predefinedName)) {
        WorldPackets::SendCharacterCreationResponse(sysAddr, CREATION_RESPONSE_PREDEFINED_NAME_IN_USE);
        return;
    }
    
	Game::logger->Log("UserManager", "AccountID: %i is creating a character with name: %s\n", u->GetAccountID(), name.c_str());
    
    //Now that the name is ok, we can get an objectID from Master:
    ObjectIDManager::Instance()->RequestPersistentID([=](uint32_t objectID) {
		sql::PreparedStatement* overlapStmt = Database::CreatePreppedStmt("SELECT id FROM charinfo WHERE id = ?");
		overlapStmt->setUInt(1, objectID);

		auto* overlapResult = overlapStmt->executeQuery();

    	if (overlapResult->next()) {
			Game::logger->Log("UserManager", "Character object id unavailable, check objectidtracker!\n");
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
        xml << "<inv><bag><b t=\"0\" m=\"20\"/><b t=\"1\" m=\"240\"/><b t=\"2\" m=\"240\"/><b t=\"3\" m=\"240\"/></bag><items><in t=\"0\">";
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
        Game::logger->Log("UserManager", "Couldn't get user to delete character\n");
        return;
    }
    
    LWOOBJID objectID = PacketUtils::ReadPacketS64(8, packet);
    objectID = GeneralUtils::ClearBit(objectID, OBJECT_BIT_CHARACTER);
    objectID = GeneralUtils::ClearBit(objectID, OBJECT_BIT_PERSISTENT);
    
    uint32_t charID = static_cast<uint32_t>(objectID);
    Game::logger->Log("UserManager", "Received char delete req for ID: %llu (%u)\n", objectID, charID);
    
    //Check if this user has this character:
    bool hasCharacter = false;
    std::vector<Character*>& characters = u->GetCharacters();
    for (size_t i = 0; i < characters.size(); ++i) {
        if (characters[i]->GetID() == charID) { hasCharacter = true; }
    }
    
    if (!hasCharacter) {
        Game::logger->Log("UserManager", "User %i tried to delete a character that it does not own!\n", u->GetAccountID());
        WorldPackets::SendCharacterDeleteResponse(sysAddr, false);
    }
    else {
        Game::logger->Log("UserManager", "Deleting character %i\n", charID);
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
			stmt->setUInt64(1, charID);
			stmt->setUInt64(2, charID);
			stmt->execute();
			delete stmt;
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
        Game::logger->Log("UserManager", "Couldn't get user to delete character\n");
        return;
    }
    
    LWOOBJID objectID = PacketUtils::ReadPacketS64(8, packet);
    objectID = GeneralUtils::ClearBit(objectID, OBJECT_BIT_CHARACTER);
    objectID = GeneralUtils::ClearBit(objectID, OBJECT_BIT_PERSISTENT);
    
    uint32_t charID = static_cast<uint32_t>(objectID);
    Game::logger->Log("UserManager", "Received char rename request for ID: %llu (%u)\n", objectID, charID);
    
    std::string newName = PacketUtils::ReadString(16, packet, true);
    
    Character* character = nullptr;
    
    //Check if this user has this character:
    bool hasCharacter = false;
    std::vector<Character*>& characters = u->GetCharacters();
    for (size_t i = 0; i < characters.size(); ++i) {
        if (characters[i]->GetID() == charID) { hasCharacter = true; character = characters[i]; }
    }
    
    if (!hasCharacter || !character) {
        Game::logger->Log("UserManager", "User %i tried to rename a character that it does not own!\n", u->GetAccountID());
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
                
                Game::logger->Log("UserManager", "Character %s now known as %s\n", character->GetName().c_str(), newName.c_str());
                WorldPackets::SendCharacterRenameResponse(sysAddr, RENAME_RESPONSE_SUCCESS);
                UserManager::RequestCharacterList(sysAddr);
            } else {
                sql::PreparedStatement* stmt = Database::CreatePreppedStmt("UPDATE charinfo SET pending_name=?, needs_rename=0, last_login=? WHERE id=? LIMIT 1");
                stmt->setString(1, newName);
                stmt->setUInt64(2, time(NULL));
                stmt->setUInt(3, character->GetID());
                stmt->execute();
                delete stmt;
                
                Game::logger->Log("UserManager", "Character %s has been renamed to %s and is pending approval by a moderator.\n", character->GetName().c_str(), newName.c_str());
                WorldPackets::SendCharacterRenameResponse(sysAddr, RENAME_RESPONSE_SUCCESS);
                UserManager::RequestCharacterList(sysAddr);
            }
        } else {
            WorldPackets::SendCharacterRenameResponse(sysAddr, RENAME_RESPONSE_NAME_IN_USE);
        }
    } else {
        Game::logger->Log("UserManager", "Unknown error occurred when renaming character, either hasCharacter or character variable != true.\n");
        WorldPackets::SendCharacterRenameResponse(sysAddr, RENAME_RESPONSE_UNKNOWN_ERROR);
    }
}

void UserManager::LoginCharacter(const SystemAddress& sysAddr, uint32_t playerID) {
    User* u = GetUser(sysAddr);
    if (!u) {
        Game::logger->Log("UserManager", "Couldn't get user to log in character\n");
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
            Game::logger->Log("UserManager", "Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i\n", character->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);
			if (character) {
				character->SetZoneID(zoneID);
				character->SetZoneInstance(zoneInstance);
				character->SetZoneClone(zoneClone);
			}
			WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
            return;
        });
    } else {
        Game::logger->Log("UserManager", "Unknown error occurred when logging in a character, either hasCharacter or character variable != true.\n");
    }
}

uint32_t GetShirtColorId(uint32_t color) {
	
	// get the index of the color in shirtColorVector
	auto colorId = std::find(shirtColorVector.begin(), shirtColorVector.end(), color);
	return color = std::distance(shirtColorVector.begin(), colorId);
}

uint32_t FindCharShirtID(uint32_t shirtColor, uint32_t shirtStyle) {
	
	shirtStyle--; // to start at 0 instead of 1
	uint32_t stylesCount = 34;
	uint32_t colorId = GetShirtColorId(shirtColor);
	
	uint32_t startID = 4049; // item ID of the shirt with color 0 (red) and style 0 (plain)
	
    // For some reason, if the shirt style is 34 - 39,
	// The ID is different than the original... Was this because
	// these shirts were added later?
	if (shirtStyle >= 34) {
		startID = 5730; // item ID of the shirt with color 0 (red) and style 34 (butterflies)
        shirtStyle -= stylesCount; //change style from range 35-40 to range 0-5
		stylesCount = 6;
	}
	
    // Get the final ID of the shirt
	uint32_t shirtID = startID + (colorId * stylesCount) + shirtStyle;

	return shirtID;
}

uint32_t FindCharPantsID(uint32_t pantsColor) {
	uint32_t pantsID = 2508;

	switch (pantsColor) {
	case 0: {
		pantsID = PANTS_BRIGHT_RED;
		break;
	}

	case 1: {
		pantsID = PANTS_BRIGHT_BLUE;
		break;
	}

	case 3: {
		pantsID = PANTS_DARK_GREEN;
		break;
	}

	case 5: {
		pantsID = PANTS_BRIGHT_ORANGE;
		break;
	}

	case 6: {
		pantsID = PANTS_BLACK;
		break;
	}

	case 7: {
		pantsID = PANTS_DARK_STONE_GRAY;
		break;
	}

	case 8: {
		pantsID = PANTS_MEDIUM_STONE_GRAY;
		break;
	}

	case 9: {
		pantsID = PANTS_REDDISH_BROWN;
		break;
	}

	case 10: {
		pantsID = PANTS_WHITE;
		break;
	}

	case 11: {
		pantsID = PANTS_MEDIUM_BLUE;
		break;
	}

	case 13: {
		pantsID = PANTS_DARK_RED;
		break;
	}

	case 14: {
		pantsID = PANTS_EARTH_BLUE;
		break;
	}

	case 15: {
		pantsID = PANTS_EARTH_GREEN;
		break;
	}

	case 16: {
		pantsID = PANTS_BRICK_YELLOW;
		break;
	}

	case 84: {
		pantsID = PANTS_SAND_BLUE;
		break;
	}

	case 96: {
		pantsID = PANTS_SAND_GREEN;
		break;
	}
	}
	
	return pantsID;
}

void UserManager::SaveAllActiveCharacters() {
	for (auto user : m_Users) {
		if (user.second) {
			auto character = user.second->GetLastUsedChar();
			if (character) character->SaveXMLToDatabase();
		}
	}
}
