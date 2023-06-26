#ifndef __RACINGCONTROLCOMPONENT__H__
#define __RACINGCONTROLCOMPONENT__H__

#include "ScriptedActivityComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class RacingControlComponent : public ScriptedActivityComponent {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::RACING_CONTROL;
	RacingControlComponent(Entity* parent, int32_t componentId);
};


#endif  //!__RACINGCONTROLCOMPONENT__H__
