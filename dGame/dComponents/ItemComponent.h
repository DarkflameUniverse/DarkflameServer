#ifndef __ITEMCOMPONENT__H__
#define __ITEMCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class Entity;

class ItemComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::ITEM;
	ItemComponent(Entity* parent);
};

#endif  //!__ITEMCOMPONENT__H__
