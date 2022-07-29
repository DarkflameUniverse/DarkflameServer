#include "WishingWellServer.h"
#include "ScriptedActivityComponent.h"
#include "GameMessages.h"

void WishingWellServer::OnStartup(Entity* self) {
}

void WishingWellServer::OnUse(Entity* self, Entity* user) {
	auto* scriptedActivity = self->GetComponent<ScriptedActivityComponent>();

	if (!scriptedActivity->TakeCost(user)) {
		return;
	}

	const auto audio = self->GetVar<std::string>(u"sound1");

	if (!audio.empty()) {
		GameMessages::SendPlayNDAudioEmitter(self, user->GetSystemAddress(), audio);
	}

	LootGenerator::Instance().DropActivityLoot(
		user,
		self,
		static_cast<uint32_t>(scriptedActivity->GetActivityID()),
		GeneralUtils::GenerateRandomNumber<int32_t>(1, 1000)
	);

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"StartCooldown", 0, 0, LWOOBJID_EMPTY, "", user->GetSystemAddress());

	const auto userID = user->GetObjectID();

	self->AddCallbackTimer(10, [self, userID]() {
		auto* user = EntityManager::Instance()->GetEntity(userID);

		if (user == nullptr) return;

		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"StopCooldown", 0, 0, LWOOBJID_EMPTY, "", user->GetSystemAddress());
		});

	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}

void WishingWellServer::OnTimerDone(Entity* self, std::string timerName) {
}
