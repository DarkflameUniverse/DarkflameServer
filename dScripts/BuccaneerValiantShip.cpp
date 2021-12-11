#include "BuccaneerValiantShip.h"
#include "SkillComponent.h"
#include "dLogger.h"

void BuccaneerValiantShip::OnStartup(Entity* self) {
    const auto skill = 982;
    const auto behavior = 20577;
    const auto skillCastTimer = 1.0F;

    self->AddCallbackTimer(skillCastTimer, [self]() {
        auto* skillComponent = self->GetComponent<SkillComponent>();
        auto* owner = self->GetOwner();

        if (skillComponent != nullptr && owner != nullptr) {
            skillComponent->CalculateBehavior(skill, behavior, LWOOBJID_EMPTY, true, false, owner->GetObjectID());

            // Kill self if missed
            const auto selfSmashTimer = 1.1F;
            self->AddCallbackTimer(selfSmashTimer, [self]() {
                self->Kill();
            });
        }
    });
}
