#ifndef OBJECTLOADSTATE_H
#define OBJECTLOADSTATE_H

#include <cstdint>
#include <initializer_list>

#include "dCommonVars.h"
#include "eReplicaComponentType.h"

class Entity;

/**
 * The client's ObjectLoadState (ObjectLoader2): the object's components as its registry gives them, after the
 * pre-load rules and the component whitelist, and what the component walk has found so far. Each component type's
 * Allocator reads it.
 */
struct ObjectLoadState {
	LOT lot{};
	// The config's componentWhitelist (ApplyComponentWhitelist): only these registry components, when set
	const std::initializer_list<eReplicaComponentType>* whitelist = nullptr;
	bool hasModelBehaviors = false;
	bool markedAsPhantom = false;
	// ApplyMovementAiIfNeeded: movement AI for a scripted object or one with an attached path
	bool addsMovementAI = false;
	// A model with behaviors' physics id, which the physics cases leave to the model case
	int32_t modelPhysicsComponentID = -1;

	// The registry's id for type, defaultValue when it has none or the whitelist drops it
	int32_t GetComponentID(eReplicaComponentType type, int32_t defaultValue = 0) const;

	// LWOScriptComponent::Allocator (0x0104d710) and LWOBaseCombatAiComponent::Allocator (0x0104c7b0): a proximity
	// monitor when there is none, and pathfinding control when the object has movement physics (controllable,
	// projectile, vehicle or havok vehicle) and none yet
	static void AddProximityAndPathfinding(Entity& entity);
};

// An allocator for a component type: makes the object's component(s) of that type (the client's LWOxComponent::Allocator)
using ComponentAllocator = void (*)(Entity& entity, ObjectLoadState& state);

#endif //!OBJECTLOADSTATE_H
