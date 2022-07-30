#include "WildAmbients.h"
#include "GameMessages.h"

void WildAmbients::OnUse(Entity* self, Entity* user) {
	GameMessages::SendPlayAnimation(self, u"interact");
}
