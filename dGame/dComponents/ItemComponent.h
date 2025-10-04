#ifndef __ITEMCOMPONENT__H__
#define __ITEMCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class ItemComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ITEM;

	ItemComponent(Entity* entity, const int32_t componentID) : Component(entity, componentID) {}

	void Serialize(RakNet::BitStream& bitStream, bool isConstruction) override;
};

#endif  //!__ITEMCOMPONENT__H__
