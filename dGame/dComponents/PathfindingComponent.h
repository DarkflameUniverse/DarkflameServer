#ifndef PATHFINDINGCOMPONENT_H
#define PATHFINDINGCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Pathfinding control, as the client's LWOPathfindingControlComponent: the client adds one (componentID -1) to a pet
 * that isn't in an inventory, and to a script or combat AI object with movement physics. Nothing of it is sent or done
 * on the server yet (the server's pathing is MovementAIComponent).
 */
class PathfindingComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PATHFINDING;

	PathfindingComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // PATHFINDINGCOMPONENT_H
