#ifndef MOVERSUBCOMPONENT_H
#define MOVERSUBCOMPONENT_H

#include "PlatformSubComponent.h"

/**
 * Standard mover - follows a pre-defined path from zone data.
 * Corresponds to client LWOPlatformMover (type 4).
 */
class MoverSubComponent final : public PlatformSubComponent {
public:
	MoverSubComponent(Entity* parentEntity, const Path* path);

private:
	bool m_AllowPosSnap = true;
	float m_MaxLerpDistance = 6.0f;
};

#endif // MOVERSUBCOMPONENT_H
