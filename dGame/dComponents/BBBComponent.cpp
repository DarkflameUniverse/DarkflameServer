#include "BBBComponent.h"
#include "Entity.h"
#include "ObjectLoadState.h"

void BBBComponent::Allocator(Entity& entity, ObjectLoadState& state) {
	// DoObjectPreLoad gives a character (LOT 1) one with id 0; any other object the registry's
	const auto id = entity.GetLOT() == 1 ? 0 : state.GetComponentID(eReplicaComponentType::BBB, -1);
	if (id > -1) entity.AddComponent<BBBComponent>(id);
}
