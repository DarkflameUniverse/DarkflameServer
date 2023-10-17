#include "DamageReduction.h"

#include "Entity.h"
#include "SkillComponent.h"

void DamageReduction::OnStartup(Entity* self) {
	auto* skillComponent = self->GetComponent<SkillComponent>();
	if (!skillComponent) return;
	skillComponent->CastSkill(m_SkillToCast, self->GetObjectID());
	if (m_DieAfter20Seconds) self->AddCallbackTimer(20.0f, [self]() { self->Smash(); });
}
