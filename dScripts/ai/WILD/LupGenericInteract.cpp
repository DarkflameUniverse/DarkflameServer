#include "LupGenericInteract.h"
#include "GameMessages.h"
#include "EffectsMessages.h"

void LupGenericInteract::OnUse(Entity* self, Entity* user) {
	GameMessages::PlayAnimation(self->GetObjectID(), u"interact").Send(UNASSIGNED_SYSTEM_ADDRESS);
}
