#include "CatapultBaseServer.h"
#include "GameMessages.h"
#include "EntityManager.h"

void CatapultBaseServer::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	if (name == "BouncerBuilt") {
		// start a timer for the arm to player the with bouncer animation
		self->AddTimer("PlatAnim", .75);

		// set the bouncer so we can use it later
		self->SetVar(u"Bouncer", sender->GetObjectID());

		GameMessages::SendBouncerActiveStatus(sender->GetObjectID(), true, UNASSIGNED_SYSTEM_ADDRESS);
	}
}

void CatapultBaseServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "PlatAnim") {
		// get the arm asset
		const auto arm = EntityManager::Instance()->GetEntitiesInGroup(self->GetVarAsString(u"ArmGroup"));

		// tell the arm to the play the platform animation, which is just the arm laying there but with bouncer
		for (auto* obj : arm) {
			GameMessages::SendPlayAnimation(obj, u"idle-platform");
			GameMessages::SendPlayNDAudioEmitter(obj, UNASSIGNED_SYSTEM_ADDRESS, "{8cccf912-69e3-4041-a20b-63e4afafc993}");
			// set the art so we can use it again
			self->SetVar(u"Arm", obj->GetObjectID());
			break;
		}

		// start a timer till the bouncer actually bounces
		self->AddTimer("bounce", 3);
	} else if (timerName == "launchAnim") {
		// get the arm asset
		auto* arm = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"Arm"));
		if (arm == nullptr) return;

		// tell the arm to player the launcher animation
		auto animTime = 1;
		self->AddTimer("resetArm", animTime);
		GameMessages::SendPlayAnimation(arm, u"launch");
	} else if (timerName == "bounce") {
		auto* bouncer = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"Bouncer"));
		if (bouncer == nullptr) return;

		// bounce all players
		bouncer->NotifyObject(bouncer, "bounceAllInProximity"); // Likely to trigger server side bounce, bodging this
		// add a delay to play the animation
		self->AddTimer("launchAnim", .3);
	} else if (timerName == "resetArm") {
		auto* arm = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"Arm"));
		if (arm == nullptr) return;

		// set the arm back to natural state
		GameMessages::SendPlayAnimation(arm, u"idle");

		auto* bouncer = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"Bouncer"));
		if (bouncer == nullptr) return;

		// kill the bouncer
		GameMessages::SendNotifyClientObject(bouncer->GetObjectID(), u"TimeToDie");
		bouncer->Smash(self->GetObjectID(), VIOLENT);
	}
}
