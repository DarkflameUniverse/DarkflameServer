#ifndef __GATERUSHCONTROLCOMPONENT__H__
#define __GATERUSHCONTROLCOMPONENT__H__

#include "RacingControlComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class GateRushComponent : public RacingControlComponent {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::GATE_RUSH_CONTROL;
	GateRushComponent(Entity* parent);
};

#endif  //!__GATERUSHCONTROLCOMPONENT__H__
