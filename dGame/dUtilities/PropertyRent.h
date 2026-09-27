#ifndef __PROPERTYRENT__H__
#define __PROPERTYRENT__H__

#include <cstdint>
#include <optional>

#include "dCommonVars.h"
#include "HotPropertySlots.h"
#include "PropertyRentRules.h"

class Entity;

/**
 * Property rent in this world (issue #943; the rules are in PropertyRentRules.h). Off unless property_rent_enabled
 * is on.
 */
namespace PropertyRent {
	bool Enabled();

	// The PropertyTemplate row a property world uses (see HotPropertySlots::WorldTemplate); nullptr when it has none
	const HotPropertySlots::TemplateRow* WorldTemplate(uint32_t mapId);

	// The world's rent: the template's, or the dashboard's price and period for it. nullopt: free
	std::optional<PropertyRentRules::Rate> RateFor(uint32_t mapId);

	// Charge the rent of every property the player owns that is due (a few seconds after their character loads, so
	// the client shows the coins and the mail), and make unpaid ones private once the grace period is over
	void OnOwnerLoaded(Entity* player);

	// Whether the property's rent is overdue, so it has to stay private
	bool IsOverdue(LWOOBJID propertyId, uint32_t mapId);
}

#endif  //!__PROPERTYRENT__H__
