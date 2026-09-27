#include "PropertyPlatform.h"
#include "QuickBuildComponent.h"
#include "GameMessages.h"
#include "MovementMessages.h"
#include "MovingPlatformComponent.h"

void PropertyPlatform::OnQuickBuildComplete(Entity* self, Entity* target) {
	//    auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
	//    if (movingPlatform != nullptr) {
	//        movingPlatform->StopPathing();
	//        movingPlatform->SetNoAutoStart(true);
	//    }
	GameMessages::PlatformResync(*self, true, 0, 0, 0, eMovementPlatformState::Stationary).Send(UNASSIGNED_SYSTEM_ADDRESS);
}

void PropertyPlatform::OnUse(Entity* self, Entity* user) {
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();
	if (quickBuildComponent != nullptr && quickBuildComponent->GetState() == eQuickBuildState::COMPLETED) {
		//        auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
		//        if (movingPlatform != nullptr) {
		//            movingPlatform->GotoWaypoint(1);
		//        }
		GameMessages::PlatformResync(*self, true, 0, 1, 1, eMovementPlatformState::Moving).Send(UNASSIGNED_SYSTEM_ADDRESS);

		self->AddCallbackTimer(movementDelay + effectDelay, [self, this]() {
			self->SetNetworkVar<float_t>(u"startEffect", dieDelay);
			self->AddCallbackTimer(dieDelay, [self]() {
				self->Smash();
				});
			});
	}
}
