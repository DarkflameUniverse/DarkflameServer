#ifndef VENDORCOMPONENT_H
#define VENDORCOMPONENT_H

#include "BaseVendorComponent.h"

struct ObjectLoadState;

/**
 * A vendor (16), as the client's LWOVendorComponent: a BaseVendorComponent.
 */
class VendorComponent final : public BaseVendorComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::VENDOR;

	// The client's allocator for this type (ObjectLoader2::DoObjectComponentLoad calls it in load order)
	static void Allocator(Entity& entity, ObjectLoadState& state);

	VendorComponent(Entity* parent, const int32_t componentID) : BaseVendorComponent(parent, componentID) {}
};

#endif // VENDORCOMPONENT_H
