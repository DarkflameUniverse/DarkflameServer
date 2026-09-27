#include "WildAndScared.h"
#include "GameMessages.h"
#include "EffectsMessages.h"

void WildAndScared::OnUse(Entity* self, Entity* user) {
	GameMessages::PlayAnimation(self->GetObjectID(), u"scared").Send(UNASSIGNED_SYSTEM_ADDRESS);
}
