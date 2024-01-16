#pragma once

// Custom Classes
#include "CDTable.h"
#include "dCommonVars.h"

struct CDItemComponent {
	uint32_t id;                        //!< The Component ID
	std::string equipLocation;         //!< The equip location
	uint32_t baseValue;                 //!< The monetary base value of the item
	bool isKitPiece;                //!< Whether or not the item belongs to a kit
	uint32_t rarity;                    //!< The rarity of the item
	uint32_t itemType;                  //!< The item type
	int64_t itemInfo;                  //!< The item info
	bool inLootTable;               //!< Whether or not the item is in a loot table
	bool inVendor;                  //!< Whether or not the item is in a vendor inventory
	bool isUnique;                  //!< ???
	bool isBOP;                     //!< ???
	bool isBOE;                     //!< ???
	uint32_t reqFlagID;                 //!< User must have completed this flag to get the item
	uint32_t reqSpecialtyID;            //!< ???
	uint32_t reqSpecRank;               //!< ???
	uint32_t reqAchievementID;          //!< The required achievement must be completed
	uint32_t stackSize;                 //!< The stack size of the item
	uint32_t color1;                    //!< Something to do with item color...
	uint32_t decal;                     //!< The decal of the item
	uint32_t offsetGroupID;             //!< Something to do with group IDs
	uint32_t buildTypes;                //!< Something to do with building
	std::string reqPrecondition;       //!< The required precondition
	uint32_t animationFlag;             //!< The Animation Flag
	uint32_t equipEffects;              //!< The effect played when the item is equipped
	bool readyForQA;                //!< ???
	uint32_t itemRating;                //!< ???
	bool isTwoHanded;               //!< Whether or not the item is double handed
	uint32_t minNumRequired;            //!< Maybe the minimum number required for a mission, or to own this object?
	uint32_t delResIndex;               //!< ???
	uint32_t currencyLOT;               //!< ???
	uint32_t altCurrencyCost;           //!< ???
	std::string subItems;              //!< A comma seperate string of sub items (maybe for multi-itemed things like faction test gear set)
	UNUSED(std::string audioEventUse);         //!< ???
	bool noEquipAnimation;          //!< Whether or not there is an equip animation
	uint32_t commendationLOT;           //!< The commendation LOT
	uint32_t commendationCost;          //!< The commendation cost
	UNUSED(std::string audioEquipMetaEventSet);    //!< ???
	std::string currencyCosts;         //!< Used for crafting
	UNUSED(std::string ingredientInfo);        //!< Unused
	uint32_t locStatus;                 //!< ???
	uint32_t forgeType;                 //!< Forge Type
	float SellMultiplier;           //!< Something to do with early vendors perhaps (but replaced)
};

class CDItemComponentTable : public CDTable<CDItemComponentTable> {
private:
	std::map<uint32_t, CDItemComponent> entries;

public:
	void LoadValuesFromDatabase();
	static std::map<LOT, uint32_t> ParseCraftingCurrencies(const CDItemComponent& itemComponent);

	// Gets an entry by ID
	const CDItemComponent& GetItemComponentByID(uint32_t skillID);

	static CDItemComponent Default;
};
