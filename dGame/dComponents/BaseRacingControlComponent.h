#ifndef __BASERACINGCONTROLCOMPONENT__H__
#define __BASERACINGCONTROLCOMPONENT__H__

#include "ScriptedActivityComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class BaseRacingControlComponent : public ScriptedActivityComponent {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::RACING_CONTROL;
	BaseRacingControlComponent(Entity* parent);
};


#endif  //!__BASERACINGCONTROLCOMPONENT__H__
