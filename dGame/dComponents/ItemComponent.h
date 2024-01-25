#ifndef __ITEMCOMPONENT__H__
#define __ITEMCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class ItemComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ITEM;

	ItemComponent(Entity* entity) : Component(entity) {}

	void Serialize(RakNet::BitStream* bitStream, bool isConstruction) override;
};

#endif  //!__ITEMCOMPONENT__H__
