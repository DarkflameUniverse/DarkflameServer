#include "PropertyPlatform.h"
#include "QuickBuildComponent.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"

void PropertyPlatform::OnQuickBuildComplete(Entity* self, Entity* target) {
	// L_PROP_PLATFORM: it stays until it is used
	if (auto* const movingPlatform = self->GetComponent<MovingPlatformComponent>()) movingPlatform->StopPathing();
}

void PropertyPlatform::OnUse(Entity* self, Entity* user) {
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();
	if (quickBuildComponent != nullptr && quickBuildComponent->GetState() == eQuickBuildState::COMPLETED) {
		if (auto* const movingPlatform = self->GetComponent<MovingPlatformComponent>()) movingPlatform->GotoWaypoint(1);

		self->AddCallbackTimer(movementDelay + effectDelay, [self, this]() {
			self->SetNetworkVar<float_t>(u"startEffect", dieDelay);
			self->AddCallbackTimer(dieDelay, [self]() {
				self->Smash();
				});
			});
	}
}
