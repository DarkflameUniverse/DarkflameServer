#include "InstanceDragon.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "eStateChangeType.h"
#include "EntityInfo.h"
#include "ProximityMonitorComponent.h"

void InstanceDragon::OnStartup(Entity* self) {
	self->SetProximityRadius(25, "Circle");
	self->SetVar<std::vector<LWOOBJID>>(u"players", {});
}

void InstanceDragon::OnUse(Entity* self, Entity* user) {

	GameMessages::SendPlayFXEffect(self->GetObjectID(), 10079, u"interact", "interact");

	self->CancelTimer("killFXEffect");
}

void InstanceDragon::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name != "Circle" || !entering->IsPlayer()) {
		return;
	}
	if (name == "Circle" && entering->IsPlayer()) {
		self->CancelTimer("killFXEffect");
	}

	auto players = self->GetVar<std::vector<LWOOBJID>>(u"players");

	const auto& iter = std::find(players.begin(), players.end(), entering->GetObjectID());

    if (players.empty() && status == "LEAVE") {
		self->AddTimer("killFXEffect", 5.0f);
	}	

	self->SetVar<std::vector<LWOOBJID>>(u"players", players);
}

void InstanceDragon::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "killFXEffect") {
		GameMessages::SendStopFXEffect(self, true, "interact");
	}
	
}

