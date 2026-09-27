#include "HydrantBroken.h"
#include "PetMessages.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "ObjectMessages.h"

void HydrantBroken::OnStartup(Entity* self) {
	self->AddTimer("playEffect", 1);

	const auto hydrant = "hydrant" + self->GetVar<std::string>(u"hydrant");

	const auto bouncers = Game::entityManager->GetEntitiesInGroup(hydrant);

	for (auto* bouncer : bouncers) {
		self->SetVar<LWOOBJID>(u"bouncer", bouncer->GetObjectID());

		GameMessages::BouncerActiveStatus msg;
		msg.target = bouncer->GetObjectID();
		msg.bActive = true;
		msg.Send(UNASSIGNED_SYSTEM_ADDRESS);

		GameMessages::NotifyObject(bouncer->GetObjectID(), self->GetObjectID(), u"enableCollision").Send(UNASSIGNED_SYSTEM_ADDRESS);
	}

	self->AddTimer("KillBroken", 25);
}

void HydrantBroken::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "KillBroken") {
		auto* bouncer = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(u"bouncer"));

		if (bouncer != nullptr) {
			GameMessages::BouncerActiveStatus msg;
			msg.target = bouncer->GetObjectID();
			msg.bActive = false;
			msg.Send(UNASSIGNED_SYSTEM_ADDRESS);

			GameMessages::NotifyObject(bouncer->GetObjectID(), self->GetObjectID(), u"disableCollision").Send(UNASSIGNED_SYSTEM_ADDRESS);
		}

		self->Kill();
	} else if (timerName == "playEffect") {
		GameMessages::PlayFXEffect(self->GetObjectID(), 384, u"water", "water").Send(UNASSIGNED_SYSTEM_ADDRESS);
	}
}
