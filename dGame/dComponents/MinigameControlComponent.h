#include "Component.h"

#include "eReplicaComponentType.h"

class Entity;

class MinigameControlComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::MINIGAME_CONTROL;
	MinigameControlComponent(Entity* parent);
};
