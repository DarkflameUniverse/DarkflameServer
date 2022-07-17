#pragma once

#ifndef __EITEMTYPE__H__
#define __EITEMTYPE__H__

#include <cstdint>

enum class eItemType : int32_t {
	ITEM_TYPE_UNKNOWN = -1, 			//!< An unknown item type
	ITEM_TYPE_BRICK = 1, 				//!< A brick
	ITEM_TYPE_HAT = 2, 					//!< A hat / head item
	ITEM_TYPE_HAIR = 3, 				//!< A hair item
	ITEM_TYPE_NECK = 4, 				//!< A neck item
	ITEM_TYPE_LEFT_HAND = 5, 			//!< A left handed item
	ITEM_TYPE_RIGHT_HAND = 6, 			//!< A right handed item
	ITEM_TYPE_LEGS = 7, 				//!< A pants item
	ITEM_TYPE_LEFT_TRINKET = 8, 		//!< A left handled trinket item
	ITEM_TYPE_RIGHT_TRINKET = 9, 		//!< A right handed trinket item
	ITEM_TYPE_BEHAVIOR = 10,			//!< A behavior
	ITEM_TYPE_PROPERTY = 11,			//!< A property
	ITEM_TYPE_MODEL = 12,				//!< A model
	ITEM_TYPE_COLLECTIBLE = 13,			//!< A collectible item
	ITEM_TYPE_CONSUMABLE = 14,			//!< A consumable item
	ITEM_TYPE_CHEST = 15,				//!< A chest item
	ITEM_TYPE_EGG = 16,					//!< An egg
	ITEM_TYPE_PET_FOOD = 17,			//!< A pet food item
	ITEM_TYPE_QUEST_OBJECT = 18,		//!< A quest item
	ITEM_TYPE_PET_INVENTORY_ITEM = 19,	//!< A pet inventory item
	ITEM_TYPE_PACKAGE = 20,				//!< A package
	ITEM_TYPE_LOOT_MODEL = 21,			//!< A loot model
	ITEM_TYPE_VEHICLE = 22,				//!< A vehicle
	ITEM_TYPE_CURRENCY = 23,			//!< Currency
	ITEM_TYPE_MOUNT = 24				//!< A Mount
};

#endif  //!__EITEMTYPE__H__