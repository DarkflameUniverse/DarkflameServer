#include "AgMonumentRaceGoal.h"
#include "EntityManager.h"


void AgMonumentRaceGoal::OnStartup(Entity* self) {
	self->SetProximityRadius(15, "RaceGoal");
}

void AgMonumentRaceGoal::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name == "RaceGoal" && entering->IsPlayer() && status == "ENTER") {
		auto* manager = EntityManager::Instance()->GetEntitiesInGroup("race_manager")[0];

		manager->OnFireEventServerSide(entering, "course_finish");
	}
}
