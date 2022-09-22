#pragma once

#include "Entity.h"
#include "Component.h"

/**
 * The property guard that stands on a property before it's claimed, allows entities to attempt claiming this property.
 */
class PropertyVendorComponent : public Component
{
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_PROPERTY_VENDOR;
	explicit PropertyVendorComponent(Entity* parent);

	/**
	 * Handles a use event from some entity, if the property is cleared this allows the entity to claim it
	 * @param originator the entity that triggered this event
	 */
	void OnUse(Entity* originator) override;

	/**
	 * Handles a property data query after the property has been claimed, sending information about the property to the
	 * triggering entity.
	 * @param originator the entity that triggered the event
	 * @param sysAddr the system address to send game message response to
	 */
	void OnQueryPropertyData(Entity* originator, const SystemAddress& sysAddr);

	/**
	 * Claims the property
	 * @param originator the entity that attempted to claim the property
	 * @param confirmed unused
	 * @param lot unused
	 * @param count unused
	 */
	void OnBuyFromVendor(Entity* originator, bool confirmed, LOT lot, uint32_t count);
};
