#ifndef __IPROPERTY__H__
#define __IPROPERTY__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum ePropertySortType : int32_t;

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
		float performanceCost{};
		uint32_t zoneId{};
	};

	struct PropertyLookup {
		uint32_t mapId{};
		std::string searchString;
		ePropertySortType sortChoice{};
		LWOOBJID playerId{};
		uint32_t numResults{};
		uint32_t startIndex{};
		uint32_t playerSort{};
	};

	struct PropertyEntranceResult {
		// This is the number of entries that are in the query IF it were ran without a limit.
		int32_t totalEntriesMatchingQuery{};
		// The entries that match the query. This should only contain up to 12 entries.
		std::vector<IProperty::Info> entries;
	};

	// Get the property info for the given property id.
	virtual std::optional<IProperty::Info> GetPropertyInfo(const LWOOBJID id) = 0;

	// Get the property info for the given property id.
	virtual std::optional<IProperty::Info> GetPropertyInfo(const LWOMAPID mapId, const LWOCLONEID cloneId) = 0;

	// Get the properties for the given property lookup params.
	// This is expected to return a result set of up to 12 properties
	// so as not to transfer too much data at once.
	virtual IProperty::PropertyEntranceResult GetProperties(const PropertyLookup& params) = 0;

	// Update the property moderation info for the given property id.
	virtual void UpdatePropertyModerationInfo(const IProperty::Info& info) = 0;
	
	// Update the property details for the given property id.
	virtual void UpdatePropertyDetails(const IProperty::Info& info) = 0;

	// Update the last updated time for the given property id.
	virtual void UpdateLastSave(const IProperty::Info& info) = 0;

	// Update the property performance cost for the given property id.
	virtual void UpdatePerformanceCost(const LWOZONEID& zoneId, const float performanceCost) = 0;
	
	// Insert a new property into the database.
	virtual void InsertNewProperty(const IProperty::Info& info, const uint32_t templateId, const LWOZONEID& zoneId) = 0;

	// Get paginated list of properties with optional search/filtering for DataTables
	// Returns a JSON-formatted string with the property data and metadata
	virtual std::string GetPropertiesTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true, bool pendingOnly = false) = 0;

	virtual uint32_t GetPropertyCount() = 0;

	// Approve a property by id
	virtual void ApproveProperty(const LWOOBJID propertyId) = 0;

	// ---- Property showcase (dashboard): only properties that are public and approved by a moderator ----

	enum class ShowcaseSort : uint8_t {
		REPUTATION, // highest first, like the game's "popular" list
		NEWEST,     // last changed first
		NAME,
	};

	struct ShowcaseQuery {
		std::string search;  // matched against the property name, description and owner's name
		uint32_t zoneId{};   // 0: every zone
		ShowcaseSort sort{ ShowcaseSort::REPUTATION };
		uint32_t start{};
		uint32_t length{ 24 };
	};

	struct ShowcaseEntry {
		IProperty::Info info;
		std::string ownerName; // the owner character's name
		uint32_t modelCount{};
	};

	struct ShowcaseResult {
		uint32_t total{}; // matching the query without start/length
		std::vector<ShowcaseEntry> entries;
	};

	// Approved (mod_approved = 1) public (privacy_option = 2) properties whose owner character still exists
	virtual ShowcaseResult GetShowcaseProperties(const ShowcaseQuery& query) = 0;
};
#endif  //!__IPROPERTY__H__
