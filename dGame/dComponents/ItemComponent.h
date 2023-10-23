#ifndef __ITEMCOMPONENT__H__
#define __ITEMCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class ItemComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::ITEM;

	ItemComponent(Entity* entity) : Component(entity) {}

	void Serialize(RakNet::BitStream* bitStream, bool isConstruction) override;
};

#endif  //!__ITEMCOMPONENT__H__
