#ifndef __IPROPERTYREPUTATIONCONTRIBUTION__H__
#define __IPROPERTYREPUTATIONCONTRIBUTION__H__

#include <cstdint>
#include <string>
#include <vector>

#include "dCommonVars.h"

class IPropertyReputationContribution {
public:
	struct ContributionInfo {
		LWOOBJID playerId{};
		uint32_t reputationGained{};
	};

	// Get today's reputation contributions for a property.
	virtual std::vector<ContributionInfo> GetPropertyReputationContributions(
		const LWOOBJID propertyId, const std::string& date) = 0;

	// Upsert a player's reputation contribution for a property on a given date.
	virtual void UpdatePropertyReputationContribution(
		const LWOOBJID propertyId, const LWOOBJID playerId,
		const std::string& date, const uint32_t reputationGained) = 0;

	// Update the total reputation on a property.
	virtual void UpdatePropertyReputation(const LWOOBJID propertyId, const uint32_t reputation) = 0;
};

#endif  //!__IPROPERTYREPUTATIONCONTRIBUTION__H__
