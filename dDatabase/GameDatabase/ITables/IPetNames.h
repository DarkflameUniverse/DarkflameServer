#ifndef __IPETNAMES__H__
#define __IPETNAMES__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dCommonVars.h"

class IPetNames {
public:
	struct Info {
		std::string petName;
		int32_t approvalStatus{};
		// The character that owns the pet (0 when not known). Saving with 0 keeps the owner already stored.
		LWOOBJID ownerId{};
	};

	// Set the pet name moderation status for the given pet id.
	virtual void SetPetNameModerationStatus(const LWOOBJID& petId, const IPetNames::Info& info) = 0;

	// Get pet info for the given pet id.
	virtual std::optional<IPetNames::Info> GetPetNameInfo(const LWOOBJID& petId) = 0;

	// Get paginated pet names data for DataTables display
	virtual std::string GetPetNamesTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true, bool pendingOnly = false) = 0;

	// Approve a pet name by id
	virtual void ApprovePetName(const int64_t id) = 0;

	// Reject (delete) a pet name by id
	virtual void RejectPetName(const int64_t id) = 0;

	// Pets whose owner was never recorded (named before owners were saved), and recording one (0: nobody has it)
	virtual std::vector<LWOOBJID> GetPetsWithUnknownOwner() = 0;
	virtual void SetPetOwner(const LWOOBJID petId, const LWOOBJID ownerId) = 0;
};

#endif  //!__IPETNAMES__H__
