#include "RaceFireballs.h"
#include "SkillComponent.h"

void RaceFireballs::OnStartup(Entity* self) {
	self->AddTimer("fire", GeneralUtils::GenerateRandomNumber<float>(3,10));
}

void RaceFireballs::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "fire") {
		auto* skillComponent = self->GetComponent<SkillComponent>();
		if (!skillComponent) {
			self->AddComponent<SkillComponent>();
		}
		skillComponent->CastSkill(894);
		self->AddTimer("fire", GeneralUtils::GenerateRandomNumber<float>(3,10));

	}
}
