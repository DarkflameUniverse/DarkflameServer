#ifndef __IPETNAMES__H__
#define __IPETNAMES__H__

#include <cstdint>
#include <optional>

class IPetNames {
public:
	struct Info {
		std::string petName;
		int32_t approvalStatus{};
	};

	// Set the pet name moderation status for the given pet id.
	virtual void SetPetNameModerationStatus(const LWOOBJID& petId, const IPetNames::Info& info) = 0;

	// Get pet info for the given pet id.
	virtual std::optional<IPetNames::Info> GetPetNameInfo(const LWOOBJID& petId) = 0;
};

#endif  //!__IPETNAMES__H__
