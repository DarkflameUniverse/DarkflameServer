#ifndef USER_H
#define USER_H

#include <string>
#include <vector>
#include "../thirdparty/raknet/Source/RakNetTypes.h"
#include "dCommonVars.h"

#include <unordered_map>

class Character;

struct BehaviorParams{
	uint32_t behavior;
	LWOOBJID objid;
	bool followup;
};

class User {
public:
	User(const SystemAddress& sysAddr, const std::string& username, const std::string& sessionKey);
	User(const User& other);
	~User();
	User& operator=(const User& other);
	bool operator==(const User& other) const;
	
	uint32_t GetAccountID() { return m_AccountID; }
	std::string& GetUsername() { return m_Username; }
	std::string& GetSessionKey() { return m_SessionKey; }
	SystemAddress& GetSystemAddress() { return m_SystemAddress; }
	
	uint32_t GetMaxGMLevel() { return m_MaxGMLevel; }
	uint32_t GetLastCharID() { return m_LastCharID; }
	void SetLastCharID(uint32_t newCharID) { m_LastCharID = newCharID; }
	
	std::vector<Character*>& GetCharacters() { return m_Characters; }
	Character* GetLastUsedChar();
    
    void SetLoggedInChar(const LWOOBJID& objID) { m_LoggedInCharID = objID; }
    LWOOBJID& GetLoggedInChar() { return m_LoggedInCharID; }

	bool GetLastChatMessageApproved() { return m_LastChatMessageApproved; }
	void SetLastChatMessageApproved(bool approved) { m_LastChatMessageApproved = approved; }

	std::unordered_map<std::string, bool> GetIsBestFriendMap() { return m_IsBestFriendMap; }
	void SetIsBestFriendMap(std::unordered_map<std::string, bool> mapToSet) { m_IsBestFriendMap = mapToSet; }

	bool GetIsMuted() const;

	time_t GetMuteExpire() const;
	void SetMuteExpire(time_t value);

	// Added for GameMessageHandler
	std::unordered_map<uint32_t, BehaviorParams> uiBehaviorHandles;
	
	void UserOutOfSync();

private:
	uint32_t m_AccountID;
	std::string m_Username;
	std::string m_SessionKey;
	SystemAddress m_SystemAddress;
	
	uint32_t m_MaxGMLevel; //The max GM level this account can assign to it's characters
	uint32_t m_LastCharID;
	std::vector<Character*> m_Characters;
    LWOOBJID m_LoggedInCharID;

	std::unordered_map<std::string, bool> m_IsBestFriendMap;

	bool m_LastChatMessageApproved = false;
	int m_AmountOfTimesOutOfSync = 0;
	const int m_MaxDesyncAllowed = 12;
	time_t m_MuteExpire;
};

#endif // USER_H
