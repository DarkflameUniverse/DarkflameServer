#include "AgSalutingNpcs.h"
#include "RenderComponent.h"

void AgSalutingNpcs::OnEmoteReceived(Entity* self, const int32_t emote, Entity* target) {
	if (emote != 356) {
		return;
	}

	RenderComponent::PlayAnimation(self, u"salutePlayer");
}
