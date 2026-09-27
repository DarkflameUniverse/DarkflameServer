#ifndef __CONTRABAND__H__
#define __CONTRABAND__H__

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "eInventoryType.h"
#include "eLootSourceType.h"
#include "IContraband.h"

class Entity;
enum class eGameMasterLevel : uint8_t;

/**
 * Contraband (issue #1563): items staff don't want players to have, listed on the dashboard's Contraband page
 * (IContraband). This world loads the list when it is first needed and again when the dashboard changes it
 * (ePlayerAction::RELOAD_CONTRABAND).
 *
 * - When a character loads (before it is sent to the client), every inventory (vault included) is checked. Each
 *   listed item is flagged (an economy flag of kind CONTRABAND, once per item). Items whose entry says so are removed
 *   too: a snapshot of the character is kept first (so the dashboard's "Give back lost items" can return them), the
 *   removal is audited and the player gets a mail saying why.
 * - When a listed item is added (loot, trade, mail, vendors, ...), the character is flagged for that item for the
 *   day, and an item to remove is removed right after it arrives, with a chat message saying why.
 *
 * Staff accounts (GM level above civilian) are not checked unless contraband_ignore_staff is 0.
 */
namespace Contraband {
	struct Entry {
		std::string reason;
		IContraband::eContrabandAction action{};
	};

	using List = std::map<LOT, Entry>;

	struct HeldItem {
		LWOOBJID id{};
		LOT lot{};
		uint32_t count{};
		eInventoryType inventory{};
	};

	struct Finding {
		HeldItem item;
		Entry entry;
	};

	// ---- Pure rules, unit tested ----

	// The held items that are on the list, in the order given
	std::vector<Finding> Find(const std::vector<HeldItem>& items, const List& list);

	// Whether an account at this GM level is checked
	bool Applies(eGameMasterLevel accountLevel, bool ignoreStaff);

	// Whether an added item should be checked: moves between a player's own inventories are not new items
	bool CountsAsAdded(eLootSourceType source, eInventoryType sourceInventory);

	// ---- This world ----

	// Load the list again from the database. Returns 1 (one world reloaded).
	uint32_t Reload();

	// The list as loaded (loads it the first time)
	const List& Get();

	// Check a player's inventories when their character loads, before it is sent to the client
	void CheckOnLoad(Entity* player);

	// A listed item was added to a player's inventory (called for every add; cheap when the LOT isn't listed)
	void OnItemAdded(Entity* owner, LOT lot, uint32_t count, eLootSourceType source, eInventoryType sourceInventory);
}

#endif  //!__CONTRABAND__H__
