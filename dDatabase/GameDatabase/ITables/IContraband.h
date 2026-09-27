#ifndef __ICONTRABAND__H__
#define __ICONTRABAND__H__

#include <cstdint>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * Contraband: items staff don't want players to have (issue #1563), edited on the dashboard. World servers check a
 * character's items against the list when it loads and when items are added, and flag (an economy flag of kind
 * CONTRABAND) or also remove them.
 */
class IContraband {
public:
	enum class eContrabandAction : uint8_t {
		FLAG = 0,   // only flag the character
		REMOVE = 1  // flag the character and remove the item
	};

	struct ContrabandItem {
		LOT lot{};
		std::string reason;
		eContrabandAction action{};
		std::string addedBy;
		int64_t addedAt{};
	};

	// Every listed item, by LOT
	virtual std::vector<ContrabandItem> GetContrabandItems() = 0;

	// Insert or replace the row of item.lot
	virtual void SetContrabandItem(const ContrabandItem& item) = 0;

	// Whether there was a row to delete
	virtual bool DeleteContrabandItem(LOT lot) = 0;
};

#endif  //!__ICONTRABAND__H__
