#include "LupGenericInteract.h"
#include "GameMessages.h"

void LupGenericInteract::OnUse(Entity* self, Entity* user) {
	GameMessages::SendPlayAnimation(self, u"interact");
}
