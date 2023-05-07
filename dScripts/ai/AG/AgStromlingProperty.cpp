#include "AgStromlingProperty.h"
#include "MovementAIComponent.h"
#include "eReplicaComponentType.h"

void AgStromlingProperty::OnStartup(Entity* self) {
	auto movementInfo = MovementAIInfo{
		"Wander",
		71,
		3,
		100,
		1,
		4
	};

	auto* movementAIComponent = new MovementAIComponent(self, movementInfo);
	self->AddComponent(eReplicaComponentType::MOVEMENT_AI, movementAIComponent);
}
