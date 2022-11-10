#include "HydrantBroken.h"
#include "EntityManager.h"
#include "GameMessages.h"

void HydrantBroken::OnStartup(Entity* self) {
	self->AddTimer("playEffect", 1);

	const auto hydrant = "hydrant" + self->GetVar<std::string>(u"hydrant");

	const auto bouncers = EntityManager::Instance()->GetEntitiesInGroup(hydrant);

	for (auto* bouncer : bouncers) {
		self->SetVar<LWOOBJID>(u"bouncer", bouncer->GetObjectID());

		GameMessages::SendBouncerActiveStatus(bouncer->GetObjectID(), true, UNASSIGNED_SYSTEM_ADDRESS);

		GameMessages::SendNotifyObject(bouncer->GetObjectID(), self->GetObjectID(), u"enableCollision", UNASSIGNED_SYSTEM_ADDRESS);
	}

	self->AddTimer("KillBroken", 25);
}

void HydrantBroken::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "KillBroken") {
		auto* bouncer = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"bouncer"));

		if (bouncer != nullptr) {
			GameMessages::SendBouncerActiveStatus(bouncer->GetObjectID(), false, UNASSIGNED_SYSTEM_ADDRESS);

			GameMessages::SendNotifyObject(bouncer->GetObjectID(), self->GetObjectID(), u"disableCollision", UNASSIGNED_SYSTEM_ADDRESS);
		}

		self->Kill();
	} else if (timerName == "playEffect") {
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 384, u"water", "water", LWOOBJID_EMPTY, 1, 1, true);
	}
}
