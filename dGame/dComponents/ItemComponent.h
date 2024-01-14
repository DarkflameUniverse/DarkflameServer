#ifndef __ITEMCOMPONENT__H__
#define __ITEMCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class ItemComponent : public Component {
public:
	constexpr static const eReplicaComponentType ComponentType = eReplicaComponentType::ITEM;

	ItemComponent(const LWOOBJID& parentEntityId) noexcept : Component{ parentEntityId } {}

	void Serialize(RakNet::BitStream* bitStream, bool isConstruction) override;
};

#endif  //!__ITEMCOMPONENT__H__
