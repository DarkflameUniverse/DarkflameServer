#include "EnemyNjBuff.h"
#include "SkillComponent.h"

void EnemyNjBuff::OnStartup(Entity* self) {
	auto* skillComponent = self->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	skillComponent->CalculateBehavior(1127, 24812, self->GetObjectID(), true);
}
