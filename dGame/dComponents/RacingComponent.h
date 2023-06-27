#ifndef __RACINGCOMPONENT__H__
#define __RACINGCOMPONENT__H__

#include "RacingControlComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class RacingComponent : public RacingControlComponent {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::RACING_CONTROL;
	RacingComponent(Entity* parent, int32_t componentId);
};


#endif  //!__RACINGCOMPONENT__H__
