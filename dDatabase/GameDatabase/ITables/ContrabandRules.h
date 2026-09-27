#ifndef __CONTRABANDRULES__H__
#define __CONTRABANDRULES__H__

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "eGameMasterLevel.h"
#include "eInventoryType.h"
#include "IContraband.h"

/**
 * The pure contraband rules, shared by world servers (Contraband.h, checking characters as they load and receive
 * items) and the dashboard (checking an uploaded character XML), so both find the same items.
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

	// The held items that are on the list, in the order given
	inline std::vector<Finding> Find(const std::vector<HeldItem>& items, const List& list) {
		std::vector<Finding> found;
		if (list.empty()) return found;
		for (const auto& item : items) {
			const auto it = list.find(item.lot);
			if (it != list.end() && item.count > 0) found.push_back({ item, it->second });
		}
		return found;
	}

	// Whether an account at this GM level is checked
	inline bool Applies(eGameMasterLevel accountLevel, bool ignoreStaff) {
		return !ignoreStaff || accountLevel <= eGameMasterLevel::CIVILIAN;
	}
}

#endif  //!__CONTRABANDRULES__H__
