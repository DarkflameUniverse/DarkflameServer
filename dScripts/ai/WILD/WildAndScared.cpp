#include "WildAndScared.h"
#include "GameMessages.h"

void WildAndScared::OnUse(Entity* self, Entity* user) {
	GameMessages::SendPlayAnimation(self, u"scared");
}
