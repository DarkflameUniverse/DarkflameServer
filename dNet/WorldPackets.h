#ifndef WORLDPACKETS_H
#define WORLDPACKETS_H

#include "dCommonVars.h"
#include "BitStreamUtils.h"
#include "MessageType/World.h"

class Entity;
enum class eLanguageCodeID : int32_t {
	EN_US = 0,
	PL_US = 1,
	DE_DE = 2,
	EN_GB = 3
};

namespace WorldPackets {

	struct UIHelpTop5: public LUBitStream {
		eLanguageCodeID languageCode = eLanguageCodeID::EN_US;

		// should these be moved up to the base class?
		SystemAddress sysAddr = UNASSIGNED_SYSTEM_ADDRESS;
		Entity* player = nullptr;

		UIHelpTop5() : LUBitStream(eConnectionType::WORLD, MessageType::World::UI_HELP_TOP_5) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};
}

#endif // WORLDPACKETS_H
