#pragma once

// Custom Classes
#include "CDTable.h"
#include "dCommonVars.h"

/*!
  \file CDItemComponentTable.hpp
  \brief Contains data for the ItemComponent table
 */

 //! ItemComponent Struct
struct CDItemComponent {
	unsigned int id;                        //!< The Component ID
	std::string equipLocation;         //!< The equip location
	unsigned int baseValue;                 //!< The monetary base value of the item
	bool isKitPiece;                //!< Whether or not the item belongs to a kit
	unsigned int rarity;                    //!< The rarity of the item
	unsigned int itemType;                  //!< The item type
	int64_t itemInfo;                  //!< The item info
	bool inLootTable;               //!< Whether or not the item is in a loot table
	bool inVendor;                  //!< Whether or not the item is in a vendor inventory
	bool isUnique;                  //!< ???
	bool isBOP;                     //!< ???
	bool isBOE;                     //!< ???
	unsigned int reqFlagID;                 //!< User must have completed this flag to get the item
	unsigned int reqSpecialtyID;            //!< ???
	unsigned int reqSpecRank;               //!< ???
	unsigned int reqAchievementID;          //!< The required achievement must be completed
	unsigned int stackSize;                 //!< The stack size of the item
	unsigned int color1;                    //!< Something to do with item color...
	unsigned int decal;                     //!< The decal of the item
	unsigned int offsetGroupID;             //!< Something to do with group IDs
	unsigned int buildTypes;                //!< Something to do with building
	std::string reqPrecondition;       //!< The required precondition
	unsigned int animationFlag;             //!< The Animation Flag
	unsigned int equipEffects;              //!< The effect played when the item is equipped
	bool readyForQA;                //!< ???
	unsigned int itemRating;                //!< ???
	bool isTwoHanded;               //!< Whether or not the item is double handed
	unsigned int minNumRequired;            //!< Maybe the minimum number required for a mission, or to own this object?
	unsigned int delResIndex;               //!< ???
	unsigned int currencyLOT;               //!< ???
	unsigned int altCurrencyCost;           //!< ???
	std::string subItems;              //!< A comma seperate string of sub items (maybe for multi-itemed things like faction test gear set)
	UNUSED(std::string audioEventUse);         //!< ???
	bool noEquipAnimation;          //!< Whether or not there is an equip animation
	unsigned int commendationLOT;           //!< The commendation LOT
	unsigned int commendationCost;          //!< The commendation cost
	UNUSED(std::string audioEquipMetaEventSet);    //!< ???
	std::string currencyCosts;         //!< Used for crafting
	UNUSED(std::string ingredientInfo);        //!< Unused
	unsigned int locStatus;                 //!< ???
	unsigned int forgeType;                 //!< Forge Type
	float SellMultiplier;           //!< Something to do with early vendors perhaps (but replaced)
};

//! ItemComponent table
class CDItemComponentTable : public CDTable {
private:
	std::map<unsigned int, CDItemComponent> entries;

public:

	//! Constructor
	CDItemComponentTable(void);

	//! Destructor
	~CDItemComponentTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	static std::map<LOT, uint32_t> ParseCraftingCurrencies(const CDItemComponent& itemComponent);

	//! Gets an entry by ID
	const CDItemComponent& GetItemComponentByID(unsigned int skillID);

	static CDItemComponent Default;
};
