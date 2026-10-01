#ifndef CULLINGPLANECOMPONENT_H
#define CULLINGPLANECOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * A plane the client culls rendering with, as the client's LoadCullingPlaneComponent makes it; the server sends nothing for it.
 */
class CullingPlaneComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::CULLING_PLANE;

	CullingPlaneComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // CULLINGPLANECOMPONENT_H
