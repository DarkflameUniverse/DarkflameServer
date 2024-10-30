#ifndef ITEMCOMPONENT_H
#define ITEMCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

class ItemComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ITEM;

	ItemComponent(Entity* entity) : Component(entity) {}

	void Serialize(RakNet::BitStream& bitStream, bool isConstruction) override;
};

#endif  //!ITEMCOMPONENT_H
