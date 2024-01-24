/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef BUILDBORDERCOMPONENT_H
#define BUILDBORDERCOMPONENT_H

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"
#include "eReplicaComponentType.h"

 /**
  * Component for the build border, allowing the user to start building when interacting with it
  */
class BuildBorderComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::BUILD_BORDER;

	BuildBorderComponent(const LWOOBJID& entityParentId) noexcept;

	/**
	 * Causes the originator to start build with this entity as a reference point
	 * @param originator the entity (probably a player) that triggered the event
	 */
	void OnUse(Entity* originator) override;
private:
};

#endif // BUILDBORDERCOMPONENT_H
