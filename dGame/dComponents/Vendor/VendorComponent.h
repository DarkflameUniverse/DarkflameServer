#ifndef VENDORCOMPONENT_H
#define VENDORCOMPONENT_H

#include "BaseVendorComponent.h"

/**
 * A vendor (16), as the client's LWOVendorComponent: a BaseVendorComponent.
 */
class VendorComponent final : public BaseVendorComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::VENDOR;

	VendorComponent(Entity* parent, const int32_t componentID) : BaseVendorComponent(parent, componentID) {}
};

#endif // VENDORCOMPONENT_H
