#include "AgBugsprayer.h"
#include "SkillComponent.h"

void AgBugsprayer::OnRebuildComplete(Entity* self, Entity* target) {
	self->AddTimer("castSkill", 1);
	self->SetOwnerOverride(target->GetObjectID());
}

void AgBugsprayer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "castSkill") {
		auto* skillComponent = self->GetComponent<SkillComponent>();

		if (skillComponent == nullptr) return;

		skillComponent->CalculateBehavior(1435, 36581, LWOOBJID_EMPTY);
	}
}
