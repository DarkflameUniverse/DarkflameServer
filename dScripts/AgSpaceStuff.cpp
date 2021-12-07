#include "AgSpaceStuff.h"
#include "GeneralUtils.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "Game.h"

void AgSpaceStuff::OnStartup(Entity* self) {
	self->AddTimer("FloaterScale", 5.0f);

	EntityInfo info{};

	info.pos = { -418, 585, -30 };
	info.lot = 33;
	info.spawnerID = self->GetObjectID();

	auto* ref = EntityManager::Instance()->CreateEntity(info);

	EntityManager::Instance()->ConstructEntity(ref);
}

void AgSpaceStuff::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "FloaterScale") {
		int scaleType = GeneralUtils::GenerateRandomNumber<int>(1, 5);

		GameMessages::SendPlayAnimation(self, u"scale_0" + GeneralUtils::to_u16string(scaleType));
		self->AddTimer("FloaterPath", 0.4);
	}
	else if (timerName == "FloaterPath") {
		int pathType = GeneralUtils::GenerateRandomNumber<int>(1, 4);
		int randTime = GeneralUtils::GenerateRandomNumber<int>(20, 25);

		GameMessages::SendPlayAnimation(self, u"path_0" + (GeneralUtils::to_u16string(pathType)));
		self->AddTimer("FloaterScale", randTime);
	}
}
