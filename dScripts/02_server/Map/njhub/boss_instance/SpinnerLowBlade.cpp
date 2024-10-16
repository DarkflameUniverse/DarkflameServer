// Low blade spinner
#include "SpinnerLowBlade.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "DestroyableComponent.h"
#include "ProximityMonitorComponent.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "EntityInfo.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"
#include "SkillComponent.h"

void SpinnerLowBlade::OnStartup(Entity* self) {

	self->SetNetworkVar(u"bIsInUse", false);
	self->SetVar(u"bActive", true);

	self->SetProximityRadius(3.5, "spin_distance");

	SpawnLegs(self);	
}


void SpinnerLowBlade::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message != "NinjagoSpinEvent" || self->GetNetworkVar<bool>(u"bIsInUse")) {
		return;
	} else {

		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();

		if (proximityMonitorComponent == nullptr || !proximityMonitorComponent->IsInProximity("spin_distance", caster->GetObjectID())) {
			return;
		}
		self->SetNetworkVar(u"bIsInUse", true);		

//		Play cinematic
		auto cine = u"DartSpinners_DartSwitcher";
		GameMessages::SendPlayCinematic(caster->GetObjectID(), cine, caster->GetSystemAddress());		

			
//		End



		TriggerDrill(self);
	}
}

void SpinnerLowBlade::TriggerDrill(Entity* self) {
	
//	Move spinners	
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
	
	auto SpinnerEntities = Game::entityManager->GetEntitiesInGroup("ShootSpinner2");	
	for (auto* spinner : SpinnerEntities) {		
		if (spinner->GetLOT() != 32047) {
			spinner->NotifyObject(self, "DartSpin");				
		}
	}	
	
//	Additional piece for LowBlades damage 	
	auto ProxyEntities = Game::entityManager->GetEntitiesInGroup("LowBlades01");	
	for (auto* blade : ProxyEntities) {		
		blade->NotifyObject(self, "LowBladeDamage");				
	}	

//	Play anims	
	RenderComponent::PlayAnimation(self, u"up");
	self->AddTimer("IdleUp", 1.0f);
	
//	Check if timed spinner
	auto ResetTime = self->GetVar<int32_t>(u"reset_time");
	
	if (ResetTime >= 1) {	
		self->AddTimer("Return", ResetTime + 2.5);	
//		2.5 = rough estimate of movetime for the timed platforms using this script			
	}
	
//	Ascend sfx
	GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");	
	self->AddTimer("BladeGUID", 1.4f);		
	
}

void SpinnerLowBlade::SpawnLegs(Entity* self) {
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

void SpinnerLowBlade::OnTimerDone(Entity* self, std::string timerName) {

	if (timerName == "Return") {
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1);		
		RenderComponent::PlayAnimation(self, u"down");
		self->AddTimer("Idle", 1.0f);

		self->AddTimer("Unlock", 2.5f);
		
//		Descend sfx
		GameMessages::SendStopNDAudioEmitter(self, self->GetSystemAddress(), "{dcd06295-949b-4179-8b99-129116def406}");	
		GameMessages::SendStopNDAudioEmitter(self, self->GetSystemAddress(), "{3062c5b2-b35a-4935-863f-a8c170aa1444}");	
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");			
//		End		
		return;	
	}
	else if (timerName == "Unlock") {					
		self->SetNetworkVar(u"bIsInUse", false);
		self->SetVar(u"bActive", true);	
		
		SpawnLegs(self);			
	}	
	else if (timerName == "IdleUp") {	
		RenderComponent::PlayAnimation(self, u"idle-up");
	}
	else if (timerName == "Idle") {	
		RenderComponent::PlayAnimation(self, u"idle");
	}
	
//Handle blades GUID
	else if (timerName == "BladeGUID") {
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{dcd06295-949b-4179-8b99-129116def406}");
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{3062c5b2-b35a-4935-863f-a8c170aa1444}");			

	}
}


