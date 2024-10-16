// Gold spinner for waves

#include "SpinnerWaves.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "DestroyableComponent.h"
#include "ProximityMonitorComponent.h"
#include "MissionComponent.h"
#include "EntityInfo.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"
#include "SoundTriggerComponent.h"

Entity* SpinnerWaves::caster1 = nullptr;

void SpinnerWaves::OnStartup(Entity* self) {

	self->SetNetworkVar(u"bIsInUse", true);
	self->SetVar(u"bActive", false);
	
	self->SetProximityRadius(3.5, "spin_distance");

}

void SpinnerWaves::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {
	if (name == "WavesOver") {		
		self->AddTimer("ActivateSpinner", 3.7f);
	}
}

void SpinnerWaves::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message != "NinjagoSpinEvent" || self->GetNetworkVar<bool>(u"bIsInUse")) {
		return;
	} else {		
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();

		if (proximityMonitorComponent == nullptr || !proximityMonitorComponent->IsInProximity("spin_distance", caster->GetObjectID())) {
			return;
		}
		self->SetNetworkVar(u"bIsInUse", true);		

		GameMessages::SendPlayCinematic(caster->GetObjectID(), u"Waves_SpinnerCam", caster->GetSystemAddress());		

		TriggerDrill(self);
	}
}

void SpinnerWaves::TriggerDrill(Entity* self) {
	
//	Move spinner	
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1, 1, eMovementPlatformState::Moving);

//	Play anim	
	RenderComponent::PlayAnimation(self, u"up");
	
//	Ascend sfx
	GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");	
	
//	Notify door
	auto DoorEntity = Game::entityManager->GetEntitiesInGroup("WavesDoor");	
	for (auto* entity : DoorEntity) {	
		entity->NotifyObject(self, "WavesSpinnerDeactivated");		
	}	
	
}


void SpinnerWaves::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "ActivateSpinner") {
		self->SetNetworkVar(u"bIsInUse", false);
		self->SetVar(u"bActive", true);	
		
//		SpawnLegs function here		

		auto pos = self->GetPosition();
		auto rot = self->GetRotation();
		
		EntityInfo info{};
		info.lot = 32000;
		info.spawnerID = self->GetObjectID();
		info.rot = rot;


		const auto dir = rot.GetRightVector();
		pos.x = pos.x;
		pos.y = pos.y - 0.1;
		pos.z = pos.z;
		info.pos = pos;


		info.rot = NiQuaternion::LookAt(info.pos, self->GetPosition());
		auto* entity = Game::entityManager->CreateEntity(info);
		Game::entityManager->ConstructEntity(entity);
	}	
}




