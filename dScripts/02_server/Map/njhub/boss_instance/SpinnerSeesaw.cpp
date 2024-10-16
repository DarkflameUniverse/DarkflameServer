#include "SpinnerSeesaw.h"
#include "Entity.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "DestroyableComponent.h"
#include "ProximityMonitorComponent.h"
#include "MissionComponent.h"
#include "EntityInfo.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"
#include "SkillComponent.h"

void SpinnerSeesaw::OnStartup(Entity* self) {	
	
	self->SetProximityRadius(3.5, "spin_distance");		
	
	if (self->GetVar<bool>(u"platformStartAtEnd")) {
		self->AddTimer("MoveUp", 26);		
	} else {
		self->AddTimer("MoveDown", 26);			
	}	
}

void SpinnerSeesaw::SpawnLegs(Entity* self) {
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
	
	self->SetVar<LWOOBJID>(u"LegObj", entity->GetObjectID());
}

void SpinnerSeesaw::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message != "NinjagoSpinEvent" || self->GetNetworkVar<bool>(u"bIsInUse")) {
		return;
	}
	self->SetNetworkVar(u"bIsInUse", true);

	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
	if (proximityMonitorComponent == nullptr || !proximityMonitorComponent->IsInProximity("spin_distance", caster->GetObjectID())) {
		return;
	}

	const auto& cine = self->GetVar<std::u16string>(u"off_cinematic");
	GameMessages::SendPlayCinematic(caster->GetObjectID(), cine, caster->GetSystemAddress());
	
//	Notify spinner activated
	auto DeactivatedGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"group_deactivated_event"));
	auto DeactivatedEntities = Game::entityManager->GetEntitiesInGroup(DeactivatedGroup);	
	for (auto* entity : DeactivatedEntities) {	
		entity->NotifyObject(self, "SpinnerDeactivated");	
	}		
	
//	Check if timed
	auto ResetTime = self->GetVar<int32_t>(u"reset_time");
	if (ResetTime >= 1) {	
		self->AddTimer("ResetPair", ResetTime + 2.5);	
//		2.5 = rough estimate of movetime for platforms		
	}
	
	SpinnerAscend(self);	
}

void SpinnerSeesaw::SpinnerAscend(Entity* self) {	
	
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1);
	RenderComponent::PlayAnimation(self, u"up");
	
//	Ascend sfx
	GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");			
}	
	
void SpinnerSeesaw::SpinnerDescend(Entity* self) {	

	self->CancelTimer("ResetPair");

	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);		
	RenderComponent::PlayAnimation(self, u"down");
	
//	Descend sfx		
	GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");		

	self->AddTimer("Unlock", 4);		
}

void SpinnerSeesaw::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {		
	if (name == "SpinnerDeactivated") {	
		SpinnerDescend(self);
	} else if (name == "SpinnerReset") {	
		auto LegObj = self->GetVar<LWOOBJID>(u"LegObj");
		auto* leg = Game::entityManager->GetEntity(LegObj);
		if (leg != nullptr) {
			leg->Smash(LegObj);			
		}
		SpinnerAscend(self);			
	}
}		

void SpinnerSeesaw::OnTimerDone(Entity* self, std::string timerName) {
		
	if (timerName == "MoveUp") {	
		SpinnerAscend(self);	
	}	
	else if (timerName == "MoveDown") {	
		SpinnerDescend(self);
	}	
	else if (timerName == "Unlock") {	
		SpawnLegs(self);
		self->SetNetworkVar(u"bIsInUse", false);
		self->SetVar(u"bActive", true);		
	}	
	else if (timerName == "ResetPair") {	
	
//		Notify reset
		auto DeactivatedGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"group_deactivated_event"));
		auto DeactivatedEntities = Game::entityManager->GetEntitiesInGroup(DeactivatedGroup);	
		for (auto* entity : DeactivatedEntities) {	
			entity->NotifyObject(self, "SpinnerReset");	
		}	
		SpinnerDescend(self);	
	}
}
