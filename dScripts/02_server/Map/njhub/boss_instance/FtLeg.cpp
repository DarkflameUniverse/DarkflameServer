#include "FtLeg.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "DestroyableComponent.h"
#include "ProximityMonitorComponent.h"
#include "MissionComponent.h"
#include "EntityInfo.h"
#include "RenderComponent.h"
#include "EntityManager.h"
#include "eMissionTaskType.h"

void FtLeg::OnStartup(Entity* self) {
	self->SetNetworkVar(u"bIsInUse", false);
	self->SetVar(u"bActive", true);
	GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, u"spin", "active");
	self->SetProximityRadius(3.5, "spin_distance");

}



void FtLeg::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {

	if (message != "NinjagoSpinEvent" || self->GetNetworkVar<bool>(u"bIsInUse")) {
		return;
	}
	self->SetProximityRadius(1, "spin_distance");
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();

	if (proximityMonitorComponent == nullptr || !proximityMonitorComponent->IsInProximity("spin_distance", caster->GetObjectID())) {
		return;
	}


	self->SetNetworkVar(u"bIsInUse", true);
	TriggerDrill(self);
	
//	Mission jumble

	auto* missionComponent = caster->GetComponent<MissionComponent>();	
	if (missionComponent != nullptr) {	
		for (const auto missionID : MissionIds) {
			missionComponent->ForceProgressValue(missionID, 1, self->GetLOT());
		}
	}	




	
	

}

void FtLeg::TriggerDrill(Entity* self) {
	self->Smash(self->GetObjectID());
	
	self->AddTimer("killDrill", 0.1f);
}

void FtLeg::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "killDrill") {
		self->Smash(self->GetObjectID());
	}
}
