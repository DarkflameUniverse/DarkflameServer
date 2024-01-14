#ifndef __MINIGAMECONTROLCOMPONENT__H__
#define __MINIGAMECONTROLCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class MiniGameControlComponent final : public Component {
public:
	constexpr static const eReplicaComponentType ComponentType = eReplicaComponentType::MINI_GAME_CONTROL;

	MiniGameControlComponent(const LWOOBJID& parentEntityId) : Component{ parentEntityId } {}
	void Serialize(RakNet::BitStream* outBitStream, bool isConstruction);
};

#endif  //!__MINIGAMECONTROLCOMPONENT__H__
