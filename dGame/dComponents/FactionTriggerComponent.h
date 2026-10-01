#ifndef FACTIONTRIGGERCOMPONENT_H
#define FACTIONTRIGGERCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Faction trigger, as the client's LWOFactionTriggerComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class FactionTriggerComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::FACTION_TRIGGER;

	FactionTriggerComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // FACTIONTRIGGERCOMPONENT_H
