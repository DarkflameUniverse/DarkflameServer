#include "AgMonumentRaceCancel.h"
#include "EntityManager.h"

void AgMonumentRaceCancel::OnCollisionPhantom(Entity* self, Entity* target) {
	auto managers = EntityManager::Instance()->GetEntitiesInGroup("race_manager");
	if (!managers.empty()) {
		managers[0]->OnFireEventServerSide(target, "course_cancel");
	}
}
