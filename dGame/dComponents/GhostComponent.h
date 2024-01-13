#ifndef __GHOSTCOMPONENT__H__
#define __GHOSTCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class GhostComponent : public Component {
public:
	static inline const eReplicaComponentType ComponentType = eReplicaComponentType::GHOST;
	GhostComponent(Entity* parent);
};

#endif  //!__GHOSTCOMPONENT__H__
