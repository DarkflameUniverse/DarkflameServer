#include "Component.h"

#include "eReplicaComponentType.h"

class Entity;

class RacingStatsComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::RACING_STATS;
	RacingStatsComponent(Entity* parent);
};
