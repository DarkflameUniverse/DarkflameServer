
#ifndef __SCRIPTEDACTIVITYCOMPONENT__H__
#define __SCRIPTEDACTIVITYCOMPONENT__H__

#include "ActivityComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class ScriptedActivityComponent : public ActivityComponent {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::SCRIPTED_ACTIVITY;
	ScriptedActivityComponent(Entity* parent, int32_t componentId);
};

#endif  //!__SCRIPTEDACTIVITYCOMPONENT__H__


