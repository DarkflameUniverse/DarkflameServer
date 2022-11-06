#pragma once

#include <string>
#include <vector>

#include "BitStream.h"

#include "NiPoint3.h"
#include "dCommonVars.h"

namespace GameMessages {
	class PropertyDataMessage final
	{
	public:
		explicit PropertyDataMessage(uint32_t mapID);

		void Serialize(RakNet::BitStream& stream) const;

		std::string OwnerName = "";
		LWOOBJID OwnerId = LWOOBJID_EMPTY;

		// Temporary values
		uint32_t TemplateID = 25166;
		uint16_t ZoneId = 1150;
		uint16_t VendorMapId = 1100;
		std::string SpawnName = "AGSmallProperty";

		std::string Name = "";
		std::string Description = "";
		std::string rejectionReason = "";

		bool moderatorRequested = 0;
		LWOCLONEID cloneId = 0;
		uint32_t reputation = 0;
		uint64_t ClaimedTime = 0;
		uint64_t LastUpdatedTime = 0;

		NiPoint3 ZonePosition = { 548.0f, 406.0f, 178.0f };
		char PrivacyOption = 0;
		float MaxBuildHeight = 128.0f;
		std::vector<NiPoint3> Paths = {};
	private:
		enum RejectionStatus : uint32_t {
			REJECTION_STATUS_APPROVED = 0,
			REJECTION_STATUS_PENDING = 1,
			REJECTION_STATUS_REJECTED = 2
		};
	};
}
