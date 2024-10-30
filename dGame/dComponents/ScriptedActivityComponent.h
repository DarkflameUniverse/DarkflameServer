#ifndef SCRIPTEDACTIVITYCOMPONENT_H
#define SCRIPTEDACTIVITYCOMPONENT_H

#include "ActivityComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class ScriptedActivityComponent final : public ActivityComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::SCRIPTED_ACTIVITY;
	ScriptedActivityComponent(Entity* parent, int activityID) : ActivityComponent(parent, activityID){};
};

#endif  //!SCRIPTEDACTIVITYCOMPONENT_H
