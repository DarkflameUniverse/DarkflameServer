#ifndef __ICHARINFO__H__
#define __ICHARINFO__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ePermissionMap.h"
#include "json.hpp"

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

	// Get the id and name of every character in one query
	virtual std::vector<std::pair<LWOOBJID, std::string>> GetCharacterIdsAndNames() = 0;

	// Get characters with a pending name change, in DataTables format
	virtual nlohmann::json GetPendingNamesTable(const uint32_t start, const uint32_t length) = 0;

	virtual void SetCharacterPermissionMap(const LWOOBJID characterId, const uint64_t permissionMap) = 0;

	// Get the character info for the given character id.
	virtual std::optional<ICharInfo::Info> GetCharacterInfo(const LWOOBJID charId) = 0;

	// Get the character info for the given character name.
	virtual std::optional<ICharInfo::Info> GetCharacterInfo(const std::string_view name) = 0;
	
	// Get the character ids for the given account.
	virtual std::vector<LWOOBJID> GetAccountCharacterIds(const LWOOBJID accountId) = 0;

	// Get the total number of characters in the database.
	virtual uint32_t GetCharacterCount() = 0;

	// Insert a new character into the database.
	virtual void InsertNewCharacter(const ICharInfo::Info info) = 0;

	// Set the name of the given character.
	virtual void SetCharacterName(const LWOOBJID characterId, const std::string_view name) = 0;

	// Set the pending name of the given character.
	virtual void SetPendingCharacterName(const LWOOBJID characterId, const std::string_view name) = 0;

	// Updates the given character ids last login to be right now.
	virtual void UpdateLastLoggedInCharacter(const LWOOBJID characterId) = 0;

	virtual bool IsNameInUse(const std::string_view name) = 0;

	virtual nlohmann::json GetCharacterById(const LWOOBJID charId) = 0;
};

#endif  //!__ICHARINFO__H__
