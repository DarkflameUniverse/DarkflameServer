#ifndef __MINIGAMECONTROLCOMPONENT__H__
#define __MINIGAMECONTROLCOMPONENT__H__

#include "ActivityComponent.h"
#include "eReplicaComponentType.h"

/**
 * Minigame control (50), the zone control object's: an activity component as in the client (LWOMinigameComponent),
 * which also sends what an activity sends.
 */
class MiniGameControlComponent final : public ActivityComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MINI_GAME_CONTROL;

	MiniGameControlComponent(Entity* parent, const int32_t componentID) : ActivityComponent(parent, componentID) {}
};

#endif  //!__MINIGAMECONTROLCOMPONENT__H__
