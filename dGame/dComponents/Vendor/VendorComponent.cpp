#include "VendorComponent.h"
#include "Entity.h"
#include "ObjectLoadState.h"
#include "ProximityMonitorComponent.h"

void VendorComponent::Allocator(Entity& entity, ObjectLoadState& state) {
	const auto vendorID = state.GetComponentID(eReplicaComponentType::VENDOR);
	// The vendor allocators add a proximity monitor first when there is none
	if (vendorID > 0) {
		if (!entity.HasComponent(eReplicaComponentType::PROXIMITY_MONITOR)) entity.AddComponent<ProximityMonitorComponent>(-1);
		entity.AddComponent<VendorComponent>(vendorID);
	}
}
