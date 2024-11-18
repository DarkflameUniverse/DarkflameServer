#pragma once

#include <map>

#include "Component.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "eReplicaComponentType.h"

/**
 * Represents the launch pad that's used to select and browse properties
 */
class PropertyEntranceComponent final : public Component {
public:
	explicit PropertyEntranceComponent(Entity* parent, uint32_t componentID);
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PROPERTY_ENTRANCE;

	/**
	 * Handles an OnUse request for some other entity, rendering the property browse menu
	 * @param entity the entity that triggered the event
	 */
	void OnUse(Entity* entity) override;

	/**
	 * Handles the event triggered when the entity selects a property to visit and makes the entity to there
	 * @param entity the entity that triggered the event
	 * @param index the index of the property property
	 * @param returnToZone whether or not the entity wishes to go back to the launch zone
	 * @param sysAddr the address to send gamemessage responses to
	 */
	void OnEnterProperty(Entity* entity, uint32_t index, bool returnToZone, const SystemAddress& sysAddr);

	/**
	 * Handles a request for information on available properties when an entity lands on the property
	 * @param entity the entity that triggered the event
	 * @param includeNullAddress unused
	 * @param includeNullDescription unused
	 * @param playerOwn only query properties owned by the entity
	 * @param updateUi unused
	 * @param numResults unused
	 * @param lReputationTime unused
	 * @param sortMethod unused
	 * @param startIndex the minimum index to start the query off
	 * @param filterText property names to search for
	 * @param sysAddr the address to send gamemessage responses to
	 */
	void OnPropertyEntranceSync(Entity* entity, bool includeNullAddress, bool includeNullDescription, bool playerOwn, bool updateUi, int32_t numResults, int32_t lReputationTime, int32_t sortMethod, int32_t startIndex, std::string filterText, const SystemAddress& sysAddr);

	/**
	 * Returns the name of this property
	 * @return the name of this property
	 */
	[[nodiscard]] std::string GetPropertyName() const { return m_PropertyName; };

	/**
	 * Returns the map ID for this property
	 * @return the map ID for this property
	 */
	[[nodiscard]] LWOMAPID GetMapID() const noexcept { return m_MapID; };

private:
	/**
	 * Cache of property information that was queried for property launched, indexed by property ID
	 */
	std::map<LWOOBJID, std::vector<PropertySelectQueryProperty>> propertyQueries;

	/**
	 * The custom name for this property
	 */
	std::string m_PropertyName;

	/**
	 * The base map ID for this property (Avant Grove, etc).
	 */
	LWOMAPID m_MapID;
};
