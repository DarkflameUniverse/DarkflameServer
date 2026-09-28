#ifndef __PROPERTYBUILDERS__H__
#define __PROPERTYBUILDERS__H__

#include <cstdint>

#include "dCommonVars.h"

/**
 * Who builds on a property (docs/PropertyBuilding.md): the owner, and with property_bff_build the owner's best friends
 * while the owner is building. The rules here have no game state (unit tested); PropertyManagementComponent applies
 * them.
 */
namespace PropertyBuilders {
	// worldconfig.ini's property_bff_build
	bool BestFriendsBuild();

	// Where a player stands on the property
	struct Player {
		LWOOBJID id{};
		bool isBestFriend{}; // of the owner
		bool isBuilding{};   // in build mode on the property
	};

	/**
	 * Whether a player can build: the owner always. A best friend of the owner (with property_bff_build) while the owner
	 * is building, and after the owner stopped until they leave build mode themselves.
	 */
	bool CanBuild(const Player& player, LWOOBJID owner, bool ownerBuilding, bool bestFriendsBuild);

	// Who placed a model, from its properties_contents.placed_by (0, NULL in the database: the owner)
	LWOOBJID Placer(LWOOBJID placedBy, LWOOBJID owner);

	enum class eModelReturn {
		PICKER,      // the player taking it off placed it: into their inventory
		PLACER,      // someone else placed it and is in this world: into the placer's inventory
		PLACER_AWAY, // someone else placed it and isn't in this world: it stays on the property
		NOT_THEIRS,  // someone else placed it and the picker wants to take it apart: it stays on the property
	};

	// Where a model taken off the property goes (DeleteModelFromClient's reason, see BrickByBrick::eDeleteReason)
	eModelReturn PlanModelReturn(LWOOBJID picker, LWOOBJID placer, bool placerHere, int32_t deleteReason);
}

#endif  //!__PROPERTYBUILDERS__H__
