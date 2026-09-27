#include "ForceFieldEffect.h"

#include "Entity.h"
#include "GameMessages.h"
#include "EffectsMessages.h"

void ForceFieldEffect::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;

	GameMessages::PlayFXEffect(target->GetObjectID(), 3671, u"cast", "").Send(UNASSIGNED_SYSTEM_ADDRESS);
}
