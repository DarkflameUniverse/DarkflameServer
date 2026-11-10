#pragma once

#include "Entity.h"
#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * The property guard that stands on a property before it's claimed, allows entities to attempt claiming this property.
 */
class PropertyVendorComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PROPERTY_VENDOR;
	explicit PropertyVendorComponent(Entity* parent, const int32_t componentID);

	/**
	 * Handles a use event from some entity, if the property is cleared this allows the entity to claim it
	 * @param originator the entity that triggered this event
	 */
	bool OnRequestUse(GameMessages::RequestUseEvent& event);

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

	// PropertyRentalResponse code the client shows as "you failed to rent" with UGG_PROPRENT_Dont_have_achievement
	// (LWOPropertyVendorComponent::onPropertyRentalResponse 0x00c56d70; 0 is a rented property)
	static constexpr uint32_t RENTAL_MISSING_ACHIEVEMENT = 9;

	// The level's property_required_mission (LWOPropertyVendorComponent::LoadConfigData 0x00c2e550), 0 without one
	uint32_t GetRequiredMission() const;

	// Whether the player has finished the level's property_required_mission, which the client asks for before the
	// vendor can be used (LWOPropertyVendorComponent::onCheckUseRequirements 0x00c2e210). A vendor without the key
	// asks for nothing here.
	bool MayRent(Entity& player) const;
};
