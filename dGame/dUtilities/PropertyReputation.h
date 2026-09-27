#ifndef __PROPERTYREPUTATION__H__
#define __PROPERTYREPUTATION__H__

#include "PropertyReputationRules.h"

/**
 * Property reputation in a property world (issues #636 and #637; the algorithm is in PropertyReputationRules.h).
 * Visitors are followed per account once a minute; what they earn the property is written to
 * property_reputation_visits and added to properties.reputation, which the property lists, the news screen's
 * "Today's Top Properties" and the dashboard show. Off when property_reputation_enabled is 0.
 */
namespace PropertyReputation {
	// The settings (property_reputation_*)
	PropertyReputationRules::Params LoadParams();

	// Called every frame on property worlds; does its work once a minute
	void Tick();
}

#endif  //!__PROPERTYREPUTATION__H__
