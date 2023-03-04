/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef PROPERTYCOMPONENT_H
#define PROPERTYCOMPONENT_H

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"
#include "eReplicaComponentType.h"

struct PropertyState {
	LWOOBJID ownerID;
	LWOOBJID propertyID;
	bool rented;
};

/**
 * This component is unused and has no functionality
 */
class PropertyComponent : public Component {
public:
	static const eReplicaComponentType ComponentType = eReplicaComponentType::PROPERTY;
	explicit PropertyComponent(Entity* parentEntity);
	~PropertyComponent() override;
	[[nodiscard]] PropertyState* GetPropertyState() const { return m_PropertyState; };
private:
	PropertyState* m_PropertyState;
	std::string m_PropertyName;
};

#endif // PROPERTYCOMPONENT_H
