#ifndef __ICHARINFO__H__
#define __ICHARINFO__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ePermissionMap.h"

// Forward declare eActivityType for Activity struct
enum class eActivityType : uint32_t;

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
		// Extended fields for dashboard
		uint32_t level{};
		uint64_t uscore{};
		uint32_t zoneId{};
		uint64_t lastLogin{};
		uint64_t createdOn{};
	};

	struct Stats {
		uint64_t totalCurrencyCollected{};
		uint64_t totalBricksCollected{};
		uint64_t totalSmashables{};
		uint64_t totalQuickbuildsCompleted{};
		uint64_t totalEnemiesSmashed{};
		uint64_t totalRocketsUsed{};
		uint64_t totalMissionsCompleted{};
		uint64_t totalPetsTamed{};
	};

	struct InventoryItem {
		LWOOBJID itemId{};
		uint32_t count{};
		int32_t slot{};
	};

	struct Activity {
		uint64_t timestamp{};
		eActivityType activity{};
		uint32_t mapId{};
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

	// Get total count of characters
	virtual uint32_t GetCharacterCount() = 0;

	// Get paginated list of all characters
	virtual std::vector<Info> GetAllCharactersPaginated(
		uint32_t offset,
		uint32_t limit,
		const std::string& orderColumn = "id",
		const std::string& orderDir = "DESC"
	) = 0;

	// Get characters with pending names (for moderation)
	virtual std::vector<Info> GetCharactersWithPendingNames() = 0;

	// Update character permission map (for restrictions)
	virtual void UpdateCharacterPermissions(const LWOOBJID characterId, ePermissionMap permissions) = 0;

	// Set needs rename flag
	virtual void SetCharacterNeedsRename(const LWOOBJID characterId, bool needsRename) = 0;

	// Get character statistics
	virtual std::optional<Stats> GetCharacterStats(const LWOOBJID characterId) = 0;

	// Get character inventory
	virtual std::vector<InventoryItem> GetCharacterInventory(const LWOOBJID characterId) = 0;

	// Get character activity history
	virtual std::vector<Activity> GetCharacterActivity(const LWOOBJID characterId, uint32_t limit = 50) = 0;

	// Rescue character to a safe zone
	virtual void RescueCharacter(const LWOOBJID characterId, uint32_t zoneId) = 0;

	// Delete character and all associated data
	virtual void DeleteCharacter(const LWOOBJID characterId) = 0;
};

#endif  //!__ICHARINFO__H__
