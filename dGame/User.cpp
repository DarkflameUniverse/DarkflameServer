#include "User.h"
#include "Database.h"
#include "Character.h"
#include "dServer.h"
#include "Logger.h"
#include "Game.h"
#include "dZoneManager.h"
#include "eServerDisconnectIdentifiers.h"
#include "eGameMasterLevel.h"

User::User(const SystemAddress& sysAddr, const std::string& username, const std::string& sessionKey) {
	m_AccountID = 0;
	m_Username = "";
	m_SessionKey = "";
	m_MuteExpire = 0;

	m_MaxGMLevel = eGameMasterLevel::CIVILIAN; //The max GM level this account can assign to it's characters
	m_LastCharID = 0;

	m_SessionKey = sessionKey;
	m_SystemAddress = sysAddr;
	m_Username = username;
	m_LoggedInCharID = 0;

	m_IsBestFriendMap = std::unordered_map<std::string, bool>();

	auto userInfo = Database::Get()->GetAccountInfo(username);
	if (userInfo) {
		m_AccountID = userInfo->id;
		m_MaxGMLevel = userInfo->maxGmLevel;
		m_MuteExpire = 0; //res->getUInt64(3);
	}

	//If we're loading a zone, we'll load the last used (aka current) character:
	if (Game::server->GetZoneID() != 0) {
		auto characterList = Database::Get()->GetAccountCharacterIds(m_AccountID);
		if (!characterList.empty()) {
			const uint32_t lastUsedCharacterId = characterList.front();
			Character* character = new Character(lastUsedCharacterId, this);
			character->UpdateFromDatabase();
			m_Characters.push_back(character);
			LOG("Loaded %i as it is the last used char", lastUsedCharacterId);
		}
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
	return m_Username == other.m_Username || m_SessionKey == other.m_SessionKey || m_SystemAddress == other.m_SystemAddress;
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
		LOG("User %s was out of sync %i times out of %i, disconnecting for suspected speedhacking.", m_Username.c_str(), m_AmountOfTimesOutOfSync, m_MaxDesyncAllowed);
		Game::server->Disconnect(this->m_SystemAddress, eServerDisconnectIdentifiers::PLAY_SCHEDULE_TIME_DONE);
	}
}

void User::UpdateBestFriendValue(const std::string_view playerName, const bool newValue) {
	m_IsBestFriendMap[playerName.data()] = newValue;
}
