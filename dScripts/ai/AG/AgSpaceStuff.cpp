#include "AgSpaceStuff.h"
#include "EntityInfo.h"
#include "GeneralUtils.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "RenderComponent.h"
#include "Entity.h"

void AgSpaceStuff::OnStartup(Entity* self) {
	self->AddTimer("FloaterScale", 5.0f);
}

void AgSpaceStuff::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "FloaterScale") {
		int scaleType = GeneralUtils::GenerateRandomNumber<int>(1, 5);

		RenderComponent::PlayAnimation(self, u"scale_0" + GeneralUtils::to_u16string(scaleType));
		self->AddTimer("FloaterPath", 0.4);
	} else if (timerName == "FloaterPath") {
		int pathType = GeneralUtils::GenerateRandomNumber<int>(1, 4);
		int randTime = GeneralUtils::GenerateRandomNumber<int>(20, 25);

		RenderComponent::PlayAnimation(self, u"path_0" + (GeneralUtils::to_u16string(pathType)));
		self->AddTimer("FloaterScale", randTime);
	}
}
