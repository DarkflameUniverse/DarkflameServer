#include "PropertyPlatform.h"
#include "QuickBuildComponent.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"

void PropertyPlatform::OnRebuildComplete(Entity* self, Entity* target) {
	//    auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
	//    if (movingPlatform != nullptr) {
	//        movingPlatform->StopPathing();
	//        movingPlatform->SetNoAutoStart(true);
	//    }
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0,
		0, 0, eMovementPlatformState::Stationary);
}

void PropertyPlatform::OnUse(Entity* self, Entity* user) {
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();
	if (quickBuildComponent != nullptr && quickBuildComponent->GetState() == eRebuildState::COMPLETED) {
		//        auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
		//        if (movingPlatform != nullptr) {
		//            movingPlatform->GotoWaypoint(1);
		//        }
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0,
			1, 1, eMovementPlatformState::Moving);

		self->AddCallbackTimer(movementDelay + effectDelay, [self, this]() {
			self->SetNetworkVar<float_t>(u"startEffect", dieDelay);
			self->AddCallbackTimer(dieDelay, [self]() {
				self->Smash();
				});
			});
	}
}
