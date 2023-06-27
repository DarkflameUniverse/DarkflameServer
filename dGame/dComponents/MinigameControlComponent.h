#ifndef __MINIGAMECONTROLCOMPONENT__H__
#define __MINIGAMECONTROLCOMPONENT__H__

#include "ActivityComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class MinigameControlComponent : public ActivityComponent {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::MINIGAME_CONTROL;
	MinigameControlComponent(Entity* parent, int32_t componentId);
};

#endif  //!__MINIGAMECONTROLCOMPONENT__H__
