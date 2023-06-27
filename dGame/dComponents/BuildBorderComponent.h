/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef BUILDBORDERCOMPONENT_H
#define BUILDBORDERCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

 /**
  * Component for the build border, allowing the user to start building when interacting with it
  */
class BuildBorderComponent final : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::BUILD_BORDER;

	BuildBorderComponent(Entity* parent) : Component(parent) { };

	/**
	 * Causes the originator to start build with this entity as a reference point
	 * @param originator the entity (probably a player) that triggered the event
	 */
	void OnUse(Entity* originator) override;
};

#endif // BUILDBORDERCOMPONENT_H
