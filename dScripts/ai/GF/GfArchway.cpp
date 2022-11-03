#include "GfArchway.h"
#include "Entity.h"
#include "SkillComponent.h"

void GfArchway::OnRebuildComplete(Entity* self, Entity* target) {
	auto* skillComponent = target->GetComponent<SkillComponent>();
	if (skillComponent)	skillComponent->CalculateBehavior(SHIELDING_SKILL, SHIELDING_BEHAVIOR, target->GetObjectID(), true);
}
