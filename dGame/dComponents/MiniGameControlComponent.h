#ifndef __MINIGAMECONTROLCOMPONENT__H__
#define __MINIGAMECONTROLCOMPONENT__H__

#include "ActivityComponent.h"
#include "eReplicaComponentType.h"

class MiniGameControlComponent final : public ActivityComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MINI_GAME_CONTROL;
	MiniGameControlComponent(Entity* parent, LOT lot) : ActivityComponent(parent, lot) {}
};

#endif  //!__MINIGAMECONTROLCOMPONENT__H__
