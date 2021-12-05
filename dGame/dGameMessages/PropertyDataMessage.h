#pragma once

#include <string>
#include <vector>

#include "BitStream.h"

#include "NiPoint3.h"
#include "dCommonVars.h"

namespace GameMessages
{
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

		uint64_t ClaimedTime = 0;

		NiPoint3 ZonePosition = { 548.0f, 406.0f, 178.0f };
		char PrivacyOption = 0;
		float MaxBuildHeight = 128.0f;
		std::vector<NiPoint3> Paths = {};
	};
}