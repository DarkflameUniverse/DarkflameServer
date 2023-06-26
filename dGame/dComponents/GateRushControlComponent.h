#ifndef __GATERUSHCONTROLCOMPONENT__H__
#define __GATERUSHCONTROLCOMPONENT__H__

#include "BaseRacingControlComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class GateRushControlComponent : public BaseRacingControlComponent {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::GATE_RUSH_CONTROL;
	GateRushControlComponent(Entity* parent, int32_t componentId);
};

#endif  //!__GATERUSHCONTROLCOMPONENT__H__
