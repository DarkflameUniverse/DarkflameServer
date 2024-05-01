// Darkflame Universe
// Copyright 2024

#ifndef RIGIDBODYPHANTOMPHYSICS_H
#define RIGIDBODYPHANTOMPHYSICS_H

#include "BitStream.h"
#include "dCommonVars.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "PhysicsComponent.h"
#include "eReplicaComponentType.h"

 /**
  * Component that handles rigid bodies that can be interacted with, mostly client-side rendered. An example is the
  * bananas that fall from trees in GF.
  */
class RigidbodyPhantomPhysicsComponent : public PhysicsComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS;

	RigidbodyPhantomPhysicsComponent(Entity* parent);

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;
};

#endif // RIGIDBODYPHANTOMPHYSICS_H
