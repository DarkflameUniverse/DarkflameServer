#include "AgSurvivalMech.h"
#include "DestroyableComponent.h"

void AgSurvivalMech::OnStartup(Entity* self) {
	BaseWavesGenericEnemy::OnStartup(self);

	auto* destroyable = self->GetComponent<DestroyableComponent>();
	if (destroyable != nullptr) {
		destroyable->SetFaction(4);
	}
}

uint32_t AgSurvivalMech::GetPoints() {
	return 200;
}
