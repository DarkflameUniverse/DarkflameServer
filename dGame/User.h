#ifndef USER_H
#define USER_H

#include <string>
#include <vector>
#include <chrono>
#include "RakNetTypes.h"
#include "dCommonVars.h"
#include "eFunnessTypes.h"

#include <memory>
#include <unordered_map>

class Character;
enum class eGameMasterLevel : uint8_t;
namespace PermissionGrants { struct Held; }

struct BehaviorParams {
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

	uint32_t GetAccountID() const noexcept { return m_AccountID; }
	std::string& GetUsername() { return m_Username; }
	std::string& GetSessionKey() { return m_SessionKey; }
	SystemAddress& GetSystemAddress() { return m_SystemAddress; }

	eGameMasterLevel GetMaxGMLevel() const { return m_MaxGMLevel; }
	void SetMaxGMLevel(eGameMasterLevel value) { m_MaxGMLevel = value; }
	uint32_t GetLastCharID() { return m_LastCharID; }
	void SetLastCharID(uint32_t newCharID) { m_LastCharID = newCharID; }

	std::vector<Character*>& GetCharacters() { return m_Characters; }
	Character* GetLastUsedChar();

	void SetLoggedInChar(const LWOOBJID& objID) { m_LoggedInCharID = objID; }
	LWOOBJID& GetLoggedInChar() { return m_LoggedInCharID; }

	bool GetLastChatMessageApproved() { return m_LastChatMessageApproved; }
	void SetLastChatMessageApproved(bool approved) { m_LastChatMessageApproved = approved; }

	const std::unordered_map<std::string, bool>& GetIsBestFriendMap() { return m_IsBestFriendMap; }
	void UpdateBestFriendValue(const std::string_view playerName, const bool newValue);

	bool GetIsMuted();

	time_t GetMuteExpire() const;
	void SetMuteExpire(time_t value);

	// Added for GameMessageHandler
	std::unordered_map<uint32_t, BehaviorParams> uiBehaviorHandles;

	void UserOutOfSync(const CaughtFunness& funness);

	// The permission grants of this account and of the character they were loaded for (SlashCommandHandler loads them
	// when first needed; nullptr until then, and again after ForgetGrants when the dashboard changed them)
	const std::shared_ptr<const PermissionGrants::Held>& GetGrants() const { return m_Grants; }
	LWOOBJID GetGrantsCharacter() const { return m_GrantsCharacter; }
	void SetGrants(std::shared_ptr<const PermissionGrants::Held> grants, LWOOBJID characterId) { m_Grants = std::move(grants); m_GrantsCharacter = characterId; }
	void ForgetGrants() { m_Grants.reset(); }

private:
	uint32_t m_AccountID;
	std::string m_Username;
	std::string m_SessionKey;
	SystemAddress m_SystemAddress;

	eGameMasterLevel m_MaxGMLevel; //The max GM level this account can assign to it's characters
	uint32_t m_LastCharID;
	std::vector<Character*> m_Characters;
	LWOOBJID m_LoggedInCharID;

	std::unordered_map<std::string, bool> m_IsBestFriendMap;

	bool m_LastChatMessageApproved = false;
	int m_AmountOfTimesOutOfSync = 0;
	const int m_MaxDesyncAllowed = 12;
	uint64_t m_MuteExpire;
	std::chrono::steady_clock::time_point m_LastMuteCheck{};
	std::vector<CaughtFunness> m_CaughtFunness{};
	std::shared_ptr<const PermissionGrants::Held> m_Grants{};
	LWOOBJID m_GrantsCharacter{};
};

#endif // USER_H
