#include "PropertyPlatform.h"
#include "RebuildComponent.h"
#include "GameMessages.h"

void PropertyPlatform::OnRebuildComplete(Entity* self, Entity* target) {
	//    auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
	//    if (movingPlatform != nullptr) {
	//        movingPlatform->StopPathing();
	//        movingPlatform->SetNoAutoStart(true);
	//    }
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0,
		0, 0, MovementPlatformState::Stationary);
}

void PropertyPlatform::OnUse(Entity* self, Entity* user) {
	auto* rebuildComponent = self->GetComponent<RebuildComponent>();
	if (rebuildComponent != nullptr && rebuildComponent->GetState() == REBUILD_COMPLETED) {
		//        auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
		//        if (movingPlatform != nullptr) {
		//            movingPlatform->GotoWaypoint(1);
		//        }
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0,
			1, 1, MovementPlatformState::Moving);

		self->AddCallbackTimer(movementDelay + effectDelay, [self, this]() {
			self->SetNetworkVar<float_t>(u"startEffect", dieDelay);
			self->AddCallbackTimer(dieDelay, [self]() {
				self->Smash();
				});
			});
	}
}
