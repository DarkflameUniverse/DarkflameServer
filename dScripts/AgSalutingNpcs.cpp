#include "AgSalutingNpcs.h"
#include "GameMessages.h"


void AgSalutingNpcs::OnEmoteReceived(Entity* self, const int32_t emote, Entity* target) {
	if (emote != 356) {
		return;
	}

	GameMessages::SendPlayAnimation(self, u"salutePlayer");
}
