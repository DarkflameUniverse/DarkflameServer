#ifndef __IPETNAMES__H__
#define __IPETNAMES__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class IPetNames {
public:
	struct Info {
		std::string petName;
		int32_t approvalStatus{};
	};

	struct DetailedInfo {
		LWOOBJID id{};
		std::string petName;
		int32_t approvalStatus{};
		LWOOBJID ownerId{};
	};

	// Set the pet name moderation status for the given pet id.
	virtual void SetPetNameModerationStatus(const LWOOBJID& petId, const IPetNames::Info& info) = 0;

	// Get pet info for the given pet id.
	virtual std::optional<IPetNames::Info> GetPetNameInfo(const LWOOBJID& petId) = 0;

	// Dashboard methods
	virtual std::vector<DetailedInfo> GetAllPetNames() = 0;
	virtual std::vector<DetailedInfo> GetPetNamesByStatus(int32_t status) = 0;
	virtual void SetPetApprovalStatus(const LWOOBJID& petId, int32_t status) = 0;
	virtual uint32_t GetPendingPetNamesCount() = 0;
};

#endif  //!__IPETNAMES__H__
