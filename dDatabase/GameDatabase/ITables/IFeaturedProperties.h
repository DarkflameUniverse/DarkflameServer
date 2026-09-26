#ifndef __IFEATUREDPROPERTIES__H__
#define __IFEATUREDPROPERTIES__H__

#include <cstdint>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * What each slot of the news screen's "Today's Top Properties" panel shows, chosen on the dashboard.
 * A slot is a PropertyTemplate row (see HotPropertySlots.h); a slot without a row shows its world's top property.
 */
class IFeaturedProperties {
public:
	struct FeaturedSlot {
		uint32_t templateId{};
		uint8_t mode{};           // HotPropertySlots::eMode
		LWOOBJID propertyId{};    // the picked property (mode PICKED), 0 otherwise
		int64_t updatedAt{};
		std::string updatedBy;
		uint32_t zoneId{};        // the property world it shows a property of; 0: the slot's own
	};

	// The whole panel's settings (featured_properties_settings)
	struct FeaturedSettings {
		bool fullAuto{};          // ignore the slots' rows: the top properties across every property world
		int64_t updatedAt{};
		std::string updatedBy;
	};

	virtual std::vector<FeaturedSlot> GetFeaturedPropertySlots() = 0;

	// Insert or replace the row of slot.templateId
	virtual void SetFeaturedPropertySlot(const FeaturedSlot& slot) = 0;

	// The defaults (per slot) when nothing was saved yet
	virtual FeaturedSettings GetFeaturedPropertiesSettings() = 0;

	virtual void SetFeaturedPropertiesSettings(const FeaturedSettings& settings) = 0;
};

#endif  //!__IFEATUREDPROPERTIES__H__
