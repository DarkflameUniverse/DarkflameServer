#pragma once

#include "dCommonVars.h"
#include "LDFFormat.h"

/**
 * An item that's equipped, generally as a smaller return type than the regular Item class
 */
struct EquippedItem
{
	/**
	 * The object ID of the equipped item
	 */
	LWOOBJID id = LWOOBJID_EMPTY;

	/**
	 * The LOT of this equipped item
	 */
	LOT lot = LOT_NULL;

	/**
	 * The number of items that are stored in this slot
	 */
	uint32_t count = 0;

	/**
	 * The slot this item is stored in
	 */
	uint32_t slot = 0;

	/**
	 * The configuration of the item with any extra data
	 */
	std::vector<LDFBaseData*> config = {};
};
