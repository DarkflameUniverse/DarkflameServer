#ifndef __EDELETIONRESTRICTIONSCHECKTYPE__H__
#define __EDELETIONRESTRICTIONSCHECKTYPE__H__

#include <cstdint>

// DeletionRestrictions.checkType, as the client's eDeletionRestrictionCheckType
enum class eDeletionRestrictionsCheckType : uint32_t {
	LOTS_INCLUDED,     // can delete while the player has another item of any of these LOTs
	LOTS_EXCLUDED,     // can delete while the player has other items of all of these LOTs
	ANY_RESTRICTION,   // can delete if any of these restrictions (rows) allows it
	ALL_RESTRICTIONS,  // can delete if all of these restrictions (rows) allow it
	ZONE,              // can delete only in one of these zones
	ALWAYS_RESTRICTED, // can never delete
	MAX
};

#endif  //!__EDELETIONRESTRICTIONSCHECKTYPE__H__
