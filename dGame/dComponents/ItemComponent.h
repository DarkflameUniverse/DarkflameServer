#ifndef __ITEMCOMPONENT__H__
#define __ITEMCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

struct ObjectLoadState;

class ItemComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ITEM;

	// The client's allocator for this type (ObjectLoader2::DoObjectComponentLoad calls it in load order)
	static void Allocator(Entity& entity, ObjectLoadState& state);

	ItemComponent(Entity* entity, const int32_t componentID) : Component(entity, componentID) {}

	void Serialize(RakNet::BitStream& bitStream, bool isConstruction) override;
};

#endif  //!__ITEMCOMPONENT__H__
