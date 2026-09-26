#include "NpcNpJetpackGuy.h"

#include "Entity.h"
#include "GameMessages.h"

void NpcNpJetpackGuy::OnUse(Entity* self, Entity* user) {
	// No effect ID, the client plays the object's own effect of this type.
	GameMessages::SendPlayFXEffect(self, -1, u"launch", "", LWOOBJID_EMPTY);
}
