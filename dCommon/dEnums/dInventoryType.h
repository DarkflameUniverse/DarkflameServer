#ifndef __DINVENTORYTYPE__H__
#define __DINVENTORYTYPE__H__

#include <cstdint>

/**
 * Represents the different types of inventories an entity may have
 */
enum eInventoryType : uint32_t {
	ITEMS = 0,
	VAULT_ITEMS,
	BRICKS,
	MODELS_IN_BBB,
	TEMP_ITEMS = 4,
	MODELS,
	TEMP_MODELS,
	BEHAVIORS,
	PROPERTY_DEEDS,
	VENDOR_BUYBACK = 11,
	HIDDEN = 12, //Used for missional items
	VAULT_MODELS = 14,
	ITEM_SETS, //internal
	INVALID // made up, for internal use!!!
};

#endif  //!__DINVENTORYTYPE__H__
