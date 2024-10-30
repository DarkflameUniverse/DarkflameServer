#ifndef MINIGAMECONTROLCOMPONENT_H
#define MINIGAMECONTROLCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

class MiniGameControlComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MINI_GAME_CONTROL;

	MiniGameControlComponent(Entity* parent) : Component(parent) {}
	void Serialize(RakNet::BitStream& outBitStream, bool isConstruction);
};

#endif  //!MINIGAMECONTROLCOMPONENT_H
