#ifndef PROPERTYPLAQUECOMPONENT_H
#define PROPERTYPLAQUECOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Property plaque, as the client's LoadPropertyPlaqueComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class PropertyPlaqueComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PROPERTY_PLAQUE;

	PropertyPlaqueComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // PROPERTYPLAQUECOMPONENT_H
