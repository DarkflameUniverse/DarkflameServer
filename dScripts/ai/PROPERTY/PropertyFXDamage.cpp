#include "PropertyFXDamage.h"
#include "DestroyableComponent.h"
#include "SkillComponent.h"

void PropertyFXDamage::OnCollisionPhantom(Entity* self, Entity* target) {
	if (target == nullptr)
		return;

	auto* skills = self->GetComponent<SkillComponent>();
	auto* targetStats = target->GetComponent<DestroyableComponent>();

	if (skills != nullptr && targetStats != nullptr) {
		auto targetFactions = targetStats->GetFactionIDs();
		if (std::find(targetFactions.begin(), targetFactions.end(), 1) != targetFactions.end()) {
			skills->CalculateBehavior(11386, 692, target->GetObjectID());
		}
	}
}
