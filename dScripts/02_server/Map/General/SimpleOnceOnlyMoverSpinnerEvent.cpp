#include "SimpleOnceOnlyMoverSpinnerEvent.h"

#include "Entity.h"
#include "MovingPlatformComponent.h"

void SimpleOnceOnlyMoverSpinnerEvent::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	const auto event = self->GetVarAsString(u"name_deactivated_event");
	if (event.empty() || args != event) return;
	if (auto* const platform = self->GetComponent<MovingPlatformComponent>()) platform->GotoWaypoint(1);
}
