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
		LWOOBJID id{};
		uint32_t accountId{};
		bool needsRename{};
		LWOCLONEID cloneId{};
		ePermissionMap permissionMap{};
	};

	// Get the approved names of all characters.
	virtual std::vector<std::string> GetApprovedCharacterNames() = 0;

	// Get the character info for the given character id.
	virtual std::optional<ICharInfo::Info> GetCharacterInfo(const LWOOBJID charId) = 0;

	// Get the character info for the given character name.
	virtual std::optional<ICharInfo::Info> GetCharacterInfo(const std::string_view name) = 0;
	
	// Get the character ids for the given account.
	virtual std::vector<LWOOBJID> GetAccountCharacterIds(const LWOOBJID accountId) = 0;

	// Insert a new character into the database.
	virtual void InsertNewCharacter(const ICharInfo::Info info) = 0;

	// Set the name of the given character.
	virtual void SetCharacterName(const LWOOBJID characterId, const std::string_view name) = 0;

	// Set the pending name of the given character.
	virtual void SetPendingCharacterName(const LWOOBJID characterId, const std::string_view name) = 0;

	// Updates the given character ids last login to be right now.
	virtual void UpdateLastLoggedInCharacter(const LWOOBJID characterId) = 0;

	virtual bool IsNameInUse(const std::string_view name) = 0;
};

#endif  //!__ICHARINFO__H__
