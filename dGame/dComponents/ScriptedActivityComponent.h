#ifndef __SCRIPTEDACTIVITYCOMPONENT__H__
#define __SCRIPTEDACTIVITYCOMPONENT__H__

#include "ActivityComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class ScriptedActivityComponent : public ActivityComponent {
public:
	constexpr static const eReplicaComponentType ComponentType = eReplicaComponentType::SCRIPTED_ACTIVITY;
	ScriptedActivityComponent(const LWOOBJID& parentEntityId, int activityID) : ActivityComponent(parentEntityId, activityID){};
};

#endif  //!__SCRIPTEDACTIVITYCOMPONENT__H__
