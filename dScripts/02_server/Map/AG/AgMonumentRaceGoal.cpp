#include "AgMonumentRaceGoal.h"
#include "EntityManager.h"


void AgMonumentRaceGoal::OnStartup(Entity* self) {
	self->SetProximityRadius(15, "RaceGoal");
}

void AgMonumentRaceGoal::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name == "RaceGoal" && entering && entering->IsPlayer() && status == "ENTER") {
		auto managers = Game::entityManager->GetEntitiesInGroup("race_manager");
		if (managers.empty() || !managers.at(0)) return;
		managers.at(0)->OnFireEventServerSide(entering, "course_finish");
	}
}
