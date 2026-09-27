#ifndef __IPROPERTYRENT__H__
#define __IPROPERTYRENT__H__

#include <cstdint>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * Property rent (see dCommon/PropertyRentRules.h): the dashboard's per-world prices and what an owner's properties owe.
 */
class IPropertyRent {
public:
	struct RentRate {
		uint32_t mapId{};
		int64_t price{};        // 0: free
		int32_t periodDays{};   // 0: the PropertyTemplate's period
		std::string updatedBy;
		int64_t updatedAt{};
	};

	struct OwnedProperty {
		LWOOBJID id{};
		uint32_t zoneId{};
		int32_t privacyOption{};
		int64_t rentDue{};      // 0: never charged
		std::string name;
	};

	virtual std::vector<RentRate> GetPropertyRentRates() = 0;
	// Insert or replace the row of rate.mapId
	virtual void SetPropertyRentRate(const RentRate& rate) = 0;
	virtual bool DeletePropertyRentRate(uint32_t mapId) = 0;

	// The properties a character owns
	virtual std::vector<OwnedProperty> GetPropertiesOfOwner(LWOOBJID ownerId) = 0;
	// The property's rent_due (0 when there is no such property)
	virtual int64_t GetPropertyRentDue(LWOOBJID propertyId) = 0;
	// Record a payment (or where the grace period starts): rent_amount and rent_due
	virtual void SetPropertyRent(LWOOBJID propertyId, int64_t amount, int64_t due) = 0;
	virtual void SetPropertyPrivacy(LWOOBJID propertyId, int32_t privacyOption) = 0;
};

#endif  //!__IPROPERTYRENT__H__
