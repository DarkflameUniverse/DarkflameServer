#ifndef __ICHARINFO__H__
#define __ICHARINFO__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ePermissionMap.h"

class ICharInfo {
public:
	struct Info {
		std::string name;
		std::string pendingName;
		uint32_t id{};
		uint32_t accountId{};
		bool needsRename{};
		LWOCLONEID cloneId{};
		ePermissionMap permissionMap{};
	};

	// Get the approved names of all characters.
	virtual std::vector<std::string> GetApprovedCharacterNames() = 0;

	// Get the character info for the given character id.
	virtual std::optional<ICharInfo::Info> GetCharacterInfo(const uint32_t charId) = 0;

	// Get the character info for the given character name.
	virtual std::optional<ICharInfo::Info> GetCharacterInfo(const std::string_view name) = 0;
	
	// Get the character ids for the given account.
	virtual std::vector<uint32_t> GetAccountCharacterIds(const uint32_t accountId) = 0;

	// Insert a new character into the database.
	virtual void InsertNewCharacter(const ICharInfo::Info info) = 0;

	// Set the name of the given character.
	virtual void SetCharacterName(const uint32_t characterId, const std::string_view name) = 0;

	// Set the pending name of the given character.
	virtual void SetPendingCharacterName(const uint32_t characterId, const std::string_view name) = 0;

	// Updates the given character ids last login to be right now.
	virtual void UpdateLastLoggedInCharacter(const uint32_t characterId) = 0;
};

#endif  //!__ICHARINFO__H__
