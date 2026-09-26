#include "ForceFieldEffect.h"

#include "Entity.h"
#include "GameMessages.h"

void ForceFieldEffect::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;

	GameMessages::SendPlayFXEffect(target, 3671, u"cast", "", LWOOBJID_EMPTY);
}
