#include "WishingWellServer.h"
#include "ScriptedActivityComponent.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "ObjectMessages.h"
#include "Loot.h"
#include "EntityManager.h"
#include "eTerminateType.h"

void WishingWellServer::OnStartup(Entity* self) {
}

void WishingWellServer::OnUse(Entity* self, Entity* user) {
	auto* scriptedActivity = self->GetComponent<ScriptedActivityComponent>();

	if (!scriptedActivity->TakeCost(user)) {
		return;
	}

	const auto audio = self->GetVar<std::string>(u"sound1");

	if (!audio.empty()) {
		GameMessages::PlayNDAudioEmitter(self->GetObjectID(), audio).Send(UNASSIGNED_SYSTEM_ADDRESS);
	}

	Loot::DropActivityLoot(
		user,
		self->GetObjectID(),
		static_cast<uint32_t>(scriptedActivity->GetActivityID()),
		GeneralUtils::GenerateRandomNumber<int32_t>(1, 1000)
	);

	GameMessages::NotifyClientObject(self->GetObjectID(), u"StartCooldown", 0, 0, LWOOBJID_EMPTY, "").Send(user->GetSystemAddress());

	const auto userID = user->GetObjectID();

	self->AddCallbackTimer(10, [self, userID]() {
		auto* user = Game::entityManager->GetEntity(userID);

		if (user == nullptr) return;

		GameMessages::NotifyClientObject(self->GetObjectID(), u"StopCooldown", 0, 0, LWOOBJID_EMPTY, "").Send(user->GetSystemAddress());
		});

	GameMessages::TerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID()).Send(UNASSIGNED_SYSTEM_ADDRESS);
}

void WishingWellServer::OnTimerDone(Entity* self, std::string timerName) {
}
