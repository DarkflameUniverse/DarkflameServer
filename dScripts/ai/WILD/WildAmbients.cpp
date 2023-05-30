#include "WildAmbients.h"
#include "GameMessages.h"
#include "RenderComponent.h"

void WildAmbients::OnUse(Entity* self, Entity* user) {
	RenderComponent::PlayAnimation(self, u"interact");
}
