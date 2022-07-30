#include "User.h"
#include "Database.h"
#include "Character.h"
#include "dServer.h"
#include "dLogger.h"
#include "Game.h"
#include "dZoneManager.h"

User::User(const SystemAddress& sysAddr, const std::string& username, const std::string& sessionKey) {
	m_AccountID = 0;
	m_Username = "";
	m_SessionKey = "";

	m_MaxGMLevel = 0; //The max GM level this account can assign to it's characters
	m_LastCharID = 0;

	m_SessionKey = sessionKey;
	m_SystemAddress = sysAddr;
	m_Username = username;
	m_LoggedInCharID = 0;

	m_IsBestFriendMap = std::unordered_map<std::string, bool>();

	//HACK HACK HACK
	//This needs to be re-enabled / updated whenever the mute stuff is moved to another table.
	//This was only done because otherwise the website's account page dies and the website is waiting on a migration to wordpress.

	//sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT id, gmlevel, mute_expire FROM accounts WHERE name=? LIMIT 1;");
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT id, gm_level FROM accounts WHERE name=? LIMIT 1;");
	stmt->setString(1, username.c_str());

	sql::ResultSet* res = stmt->executeQuery();
	while (res->next()) {
		m_AccountID = res->getUInt(1);
		m_MaxGMLevel = res->getInt(2);
		m_MuteExpire = 0; //res->getUInt64(3);
	}

	delete res;
	delete stmt;

	//If we're loading a zone, we'll load the last used (aka current) character:
	if (Game::server->GetZoneID() != 0) {
		sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT id FROM charinfo WHERE account_id=? ORDER BY last_login DESC LIMIT 1;");
		stmt->setUInt(1, m_AccountID);

		sql::ResultSet* res = stmt->executeQuery();
		if (res->rowsCount() > 0) {
			while (res->next()) {
				LWOOBJID objID = res->getUInt64(1);
				Character* character = new Character(uint32_t(objID), this);
				m_Characters.push_back(character);
				Game::logger->Log("User", "Loaded %llu as it is the last used char", objID);
			}
		}

		delete res;
		delete stmt;
	}
}

User::User(const User& other) {
	this->m_AccountID = other.m_AccountID;
	this->m_LastCharID = other.m_LastCharID;
	this->m_MaxGMLevel = other.m_MaxGMLevel;
	this->m_SessionKey = other.m_SessionKey;
	this->m_SystemAddress = other.m_SystemAddress;
	this->m_Username = other.m_Username;
	this->m_LoggedInCharID = other.m_LoggedInCharID;
}

User::~User() {
	for (Character* c : m_Characters) {
		if (c) {
			delete c;
			c = nullptr;
		}
	}
}

User& User::operator= (const User& other) {
	this->m_AccountID = other.m_AccountID;
	this->m_LastCharID = other.m_LastCharID;
	this->m_MaxGMLevel = other.m_MaxGMLevel;
	this->m_SessionKey = other.m_SessionKey;
	this->m_SystemAddress = other.m_SystemAddress;
	this->m_Username = other.m_Username;
	this->m_LoggedInCharID = other.m_LoggedInCharID;
	return *this;
}

bool User::operator== (const User& other) const {
	if (m_Username == other.m_Username || m_SessionKey == other.m_SessionKey || m_SystemAddress == other.m_SystemAddress)
		return true;

	return false;
}

Character* User::GetLastUsedChar() {
	if (m_Characters.size() == 0) return nullptr;
	else if (m_Characters.size() == 1) return m_Characters[0];
	else {
		Character* toReturn = m_Characters[0];
		for (size_t i = 0; i < m_Characters.size(); ++i) {
			if (m_Characters[i]->GetLastLogin() > toReturn->GetLastLogin()) toReturn = m_Characters[i];
		}

		return toReturn;
	}
}

bool User::GetIsMuted() const {
	return m_MuteExpire == 1 || m_MuteExpire > time(NULL);
}

time_t User::GetMuteExpire() const {
	return m_MuteExpire;
}

void User::SetMuteExpire(time_t value) {
	m_MuteExpire = value;
}

void User::UserOutOfSync() {
	m_AmountOfTimesOutOfSync++;
	if (m_AmountOfTimesOutOfSync > m_MaxDesyncAllowed) {
		//YEET
		Game::logger->Log("User", "User %s was out of sync %i times out of %i, disconnecting for suspected speedhacking.", m_Username.c_str(), m_AmountOfTimesOutOfSync, m_MaxDesyncAllowed);
		Game::server->Disconnect(this->m_SystemAddress, SERVER_DISCON_KICK);
	}
}
