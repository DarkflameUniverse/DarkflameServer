#include "NpcNpJetpackGuy.h"

#include "Entity.h"
#include "GameMessages.h"
#include "EffectsMessages.h"

void NpcNpJetpackGuy::OnUse(Entity* self, Entity* user) {
	// No effect ID, the client plays the object's own effect of this type.
	GameMessages::PlayFXEffect(self->GetObjectID(), -1, u"launch", "").Send(UNASSIGNED_SYSTEM_ADDRESS);
}
