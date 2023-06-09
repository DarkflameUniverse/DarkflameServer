#include "BuccaneerValiantShip.h"
#include "SkillComponent.h"

void BuccaneerValiantShip::OnStartup(Entity* self) {
	self->AddCallbackTimer(1.0F, [self]() {
		auto* skillComponent = self->GetComponent<SkillComponent>();
		auto* owner = self->GetOwner();

		if (skillComponent != nullptr && owner != nullptr) {
			skillComponent->CalculateBehavior(982, 20577, LWOOBJID_EMPTY, true, false, owner->GetObjectID());

			// Kill self if missed
			self->AddCallbackTimer(1.1F, [self]() {
				self->Kill();
				});
		}
		});
}
