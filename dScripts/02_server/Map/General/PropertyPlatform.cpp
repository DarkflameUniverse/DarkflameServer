#include "PropertyPlatform.h"
#include "QuickBuildComponent.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"

void PropertyPlatform::OnRebuildComplete(Entity* self, Entity* target) {
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS
	, static_cast<eMovementPlatformState>(eMovementPlatformState::Waiting | eMovementPlatformState::ReachedDesiredWaypoint | eMovementPlatformState::ReachedFinalWaypoint),
	true, 0, 0, 0);
}

void PropertyPlatform::OnUse(Entity* self, Entity* user) {
	auto* rebuildComponent = self->GetComponent<RebuildComponent>();
	if (rebuildComponent != nullptr && rebuildComponent->GetState() == eRebuildState::COMPLETED) {
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, eMovementPlatformState::Travelling, true, 0,
			1, 1);

		self->AddCallbackTimer(movementDelay + effectDelay, [self, this]() {
			self->SetNetworkVar<float_t>(u"startEffect", dieDelay);
			self->AddCallbackTimer(dieDelay, [self]() {
				self->Smash();
				});
			});
	}
}
