/*
 * Darkflame Universe
 * Copyright 2024
 */

#ifndef PROPERTYCOMPONENT_H
#define PROPERTYCOMPONENT_H

#include "Entity.h"
#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * This component is unused and has no functionality
 */
class PropertyComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PROPERTY;
	explicit PropertyComponent(Entity* const parentEntity) noexcept : Component{ parentEntity } {}
};

#endif // !PROPERTYCOMPONENT_H
