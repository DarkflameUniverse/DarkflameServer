#include "PropertyPlatform.h"
#include "RebuildComponent.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"

void PropertyPlatform::OnRebuildComplete(Entity* self, Entity* target) {
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0,
		0, 0, eMovementPlatformState::Waiting | eMovementPlatformState::ReachedDesiredWaypoint | eMovementPlatformState::ReachedFinalWaypoint);
}

void PropertyPlatform::OnUse(Entity* self, Entity* user) {
	auto* rebuildComponent = self->GetComponent<RebuildComponent>();
	if (rebuildComponent != nullptr && rebuildComponent->GetState() == eRebuildState::COMPLETED) {
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0,
			1, 1, eMovementPlatformState::Travelling);

		self->AddCallbackTimer(movementDelay + effectDelay, [self, this]() {
			self->SetNetworkVar<float_t>(u"startEffect", dieDelay);
			self->AddCallbackTimer(dieDelay, [self]() {
				self->Smash();
				});
			});
	}
}
