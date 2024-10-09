#include "SpinnerHighBlade.h"
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

void SpinnerHighBlade::OnStartup(Entity* self) {	
	
	self->SetProximityRadius(3.5, "spin_distance");		
	
	if (self->GetVar<bool>(u"platformStartAtEnd")) {
		self->AddTimer("MoveUp", 26);		
	} else {
		self->AddTimer("MoveDown", 26);			
	}	
}

void SpinnerHighBlade::SpawnLegs(Entity* self) {
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

void SpinnerHighBlade::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
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
		self->AddTimer("MoveDown", ResetTime + 2.5);	
//		2.5 = rough estimate of movetime for platforms		
	}
	
	SpinnerAscend(self);	
}

void SpinnerHighBlade::SpinnerAscend(Entity* self) {	
	
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1);
	RenderComponent::PlayAnimation(self, u"up");
	
//	Fake skill pulse to avoid instant user damage
	self->AddTimer("SkillPulse", 1.6f);
	self->AddTimer("SkillPulse", 2.6f);
	self->AddTimer("SkillPulse", 3.6f);	
	
	self->AddTimer("IdleAnim", 1);	
	self->AddTimer("BladeRadius", 3.5f);
	
//	Ascend sfx
	GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{5c30c263-00ae-42a2-80a3-2ae33c8f13fe}");	
	self->AddTimer("AscentGUID", 0.1f);		
}	
	
void SpinnerHighBlade::SpinnerDescend(Entity* self) {	

	self->SetVar<bool>(u"SpinnerIsUp", false);
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);		
	RenderComponent::PlayAnimation(self, u"down");
	
//	Descend sfx
	GameMessages::SendStopNDAudioEmitter(self, self->GetSystemAddress(), "{ab21b048-5d1a-40b3-9203-88b376f92087}");		
	GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{40e86d71-084c-4149-884e-ab9b45b694dc}");	
	self->AddTimer("DescentGUID", 0.1f);	

	self->AddTimer("Unlock", 4);		
}

void SpinnerHighBlade::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
// Damage players & enemies via proximity radius 	
	if (name == "damage_distance") {
		if (self->GetVar<bool>(u"SpinnerIsUp")) {				
			if (entering->IsPlayer()) {
				auto* skillComponent = self->GetComponent<SkillComponent>();
				auto* skillComponentPlayer = entering->GetComponent<SkillComponent>();
				
				if (skillComponent == nullptr) {
					return;
				}

				skillComponentPlayer->CalculateBehavior(99994, 99994, entering->GetObjectID(), true);	

				auto dir = entering->GetRotation().GetForwardVector();
				dir.y = 11;
				dir.x = -dir.x * 14;
				dir.z = -dir.z * 14;
				GameMessages::SendKnockback(entering->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);
			}
		}
	}
}

void SpinnerHighBlade::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {		
	if (name == "SpinnerDeactivated") {	
		SpinnerDescend(self);
	}
}		

void SpinnerHighBlade::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "SkillPulse") {	
// Spinner damage skill exclusive for enemies	
		auto* skillComponent = self->GetComponent<SkillComponent>();

		if (skillComponent == nullptr) {
			return;
		}

		skillComponent->CalculateBehavior(971, 20371, self->GetObjectID(), true); 
//		effectID from behaviorID 20370 removed -> inaccurate offset & annoying
		if (self->GetVar<bool>(u"SpinnerIsUp")) {
			self->AddTimer("SkillPulse", 1);
		}		
	}
	else if (timerName == "BladeRadius") {		
		self->SetVar<bool>(u"SpinnerIsUp", true);
//		^^ If ProxRadius activated, should be true anyways		
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
		self->SetProximityRadius(5.9, "damage_distance");			
	}			
	else if (timerName == "IdleAnim") {	
		RenderComponent::PlayAnimation(self, u"idle-up");
	}	
	else if (timerName == "MoveUp") {	
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
//	Handle spinner sound orders
	else if (timerName == "AscentGUID") {
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");	
		self->AddTimer("BladeGUID", 1.4f);		
	}
	else if (timerName == "DescentGUID") {
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");	
	}
	else if (timerName == "BladeGUID") {
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{ab21b048-5d1a-40b3-9203-88b376f92087}");	

	}		
}
