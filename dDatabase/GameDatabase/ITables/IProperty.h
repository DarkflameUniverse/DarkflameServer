#ifndef __IPROPERTY__H__
#define __IPROPERTY__H__

#include <cstdint>
#include <optional>

class IProperty {
public:
	struct Info {
		std::string name;
		std::string description;
		std::string rejectionReason;
		LWOOBJID id{};
		LWOOBJID ownerId{};
		LWOCLONEID cloneId{};
		int32_t privacyOption{};
		uint32_t modApproved{};
		uint32_t lastUpdatedTime{};
		uint32_t claimedTime{};
		uint32_t reputation{};
	};

	// Get the property info for the given property id.
	virtual std::optional<IProperty::Info> GetPropertyInfo(const LWOMAPID mapId, const LWOCLONEID cloneId) = 0;

	// Update the property moderation info for the given property id.
	virtual void UpdatePropertyModerationInfo(const IProperty::Info& info) = 0;
	
	// Update the property details for the given property id.
	virtual void UpdatePropertyDetails(const IProperty::Info& info) = 0;

	// Update the property performance cost for the given property id.
	virtual void UpdatePerformanceCost(const LWOZONEID& zoneId, const float performanceCost) = 0;
	
	// Insert a new property into the database.
	virtual void InsertNewProperty(const IProperty::Info& info, const uint32_t templateId, const LWOZONEID& zoneId) = 0;
};
#endif  //!__IPROPERTY__H__
