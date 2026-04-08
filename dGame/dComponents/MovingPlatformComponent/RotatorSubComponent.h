#ifndef ROTATORSUBCOMPONENT_H
#define ROTATORSUBCOMPONENT_H

#include "PlatformSubComponent.h"

/**
 * Rotator - follows a path like Mover but also applies angular velocity.
 * Corresponds to client LWOPlatformRotator (type 6).
 */
class RotatorSubComponent final : public PlatformSubComponent {
public:
	RotatorSubComponent(Entity* parentEntity, const Path* path);

	void UpdatePositionAlongPath(float deltaTime) override;

private:
	NiPoint3 m_RotationAxis{};
	float m_Rate = 0.0f;
	NiPoint3 m_AngularVelocity{};
	bool m_AllowRotSnap = true;
	float m_MaxLerpAngle = 0.785398f; // ~45 degrees
};

#endif // ROTATORSUBCOMPONENT_H
