/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef __PROPERTYCOMPONENT_H__
#define __PROPERTYCOMPONENT_H__

#include "Component.h"
#include "eReplicaComponentType.h"

class PropertyComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::PROPERTY;
	PropertyComponent(Entity* parentEntity);
};

#endif // __PROPERTYCOMPONENT_H__
