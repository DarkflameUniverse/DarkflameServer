#ifndef IPETNAMES_H
#define IPETNAMES_H

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

#endif  //!IPETNAMES_H
