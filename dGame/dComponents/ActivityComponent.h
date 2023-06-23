#ifndef __ACTIVITYCOMPONENT__H__
#define __ACTIVITYCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class Entity;

class ActivityComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::INVALID;
	ActivityComponent(Entity* parent);
};

#endif  //!__ACTIVITYCOMPONENT__H__


