#ifndef __IPROPERTY__H__
#define __IPROPERTY__H__

#include <cstdint>
#include <optional>

enum ePropertySortType : int32_t;

class IProperty {
public:
	struct Info {
		std::string name;
		std::string description;
		std::string rejectionReason;
		LWOOBJID id{};
		uint32_t ownerId{};
		LWOCLONEID cloneId{};
		int32_t privacyOption{};
		uint32_t modApproved{};
		uint32_t lastUpdatedTime{};
		uint32_t claimedTime{};
		uint32_t reputation{};
		float performanceCost{};
	};

	struct PropertyLookup {
		uint32_t mapId{};
		std::string searchString;
		ePropertySortType sortChoice{};
		uint32_t playerId{};
		uint32_t numResults{};
		uint32_t startIndex{};
		uint32_t playerSort{};
	};

	struct PropertyEntranceResult {
		int32_t totalEntriesMatchingQuery{};
		// The entries that match the query. This should only contain up to 12 entries.
		std::vector<IProperty::Info> entries;
	};

	// Get the property info for the given property id.
	virtual std::optional<IProperty::Info> GetPropertyInfo(const LWOMAPID mapId, const LWOCLONEID cloneId) = 0;

	// Get the properties for the given property lookup params.
	// This is expected to return a result set of up to 12 properties
	// so as not to transfer too much data at once.
	virtual std::optional<IProperty::PropertyEntranceResult> GetProperties(const PropertyLookup& params) = 0;

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
