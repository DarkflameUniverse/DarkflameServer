#include "SpinnerHighBladeGen.h"
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

void SpinnerHighBladeGen::OnStartup(Entity* self) {	
	self->SetNetworkVar(u"bIsInUse", false);
	self->SetVar(u"bActive", true);
	
	self->SetProximityRadius(3.5, "spin_distance");	

	if (self->GetVar<std::u16string>(u"toggle") == u"toggle_activate") {
		self->SetVar<int>(u"ToggleSpinner", 1);	
	} 		
	
	if (self->GetVar<bool>(u"platformStartAtEnd")) {
//		Immediate visuals for entrance spinners		
		const auto groupID = self->GetVar<std::u16string>(u"groupID");
		const auto AttachedPath = self->GetVar<std::u16string>(u"attached_path");
		
		self->SetVar<bool>(u"SpinnerIsUp", false);	
		
		if (self->GetVar<std::u16string>(u"groupID") == u"room_1_blades;") {	
		
			self->SetVar<bool>(u"SpinnerIsUp", true);	
			self->SetProximityRadius(5.9, "damage_distance");
			self->AddTimer("EntranceAnimLoop", 3);	
			
			if (AttachedPath == u"ZSpinner03") {
				self->AddTimer("BladeGUID", 11);	
			} 		
			return;
		} else {
			self->SetVar<bool>(u"SpinnerIsUp", false);				
		}	
	} else {
		self->SetVar<bool>(u"SpinnerIsUp", true);				
	}
	self->AddTimer("Return", 26);		
}

void SpinnerHighBladeGen::SpinnerAscend(Entity* self) {	
	
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1);
	RenderComponent::PlayAnimation(self, u"up");
	
//	Fake skill pulse to avoid instant user damage
	self->AddTimer("SkillPulse", 1.6f);
	self->AddTimer("SkillPulse", 2.6f);
	self->AddTimer("SkillPulse", 3.6f);	
	
	self->AddTimer("IdleAnim", 1);	
	self->AddTimer("BladeRadius", 3.5f);
	
//	Ascend sfx
	GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");	
	self->AddTimer("BladeGUID", 1.4f);		
}	
	
void SpinnerHighBladeGen::SpinnerDescend(Entity* self) {	

	self->SetVar<bool>(u"SpinnerIsUp", false);
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);		
	RenderComponent::PlayAnimation(self, u"down");


	self->SetNetworkVar(u"bIsInUse", false);
	self->SetVar(u"bActive", true);
	
//	Descend sfx
	const auto AttachedPath = self->GetVar<std::u16string>(u"attached_path");
	if (AttachedPath == u"ZSpinner53") {
		GameMessages::SendStopNDAudioEmitter(self, self->GetSystemAddress(), "{43d11dc0-a096-4595-8fbe-f95a4a0d951e}");	
	} else {
		GameMessages::SendStopNDAudioEmitter(self, self->GetSystemAddress(), "{b1bbe65e-330d-4ef6-a534-63e98dd199ec}");
	}	
	GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");	
	
}

void SpinnerHighBladeGen::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
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

void SpinnerHighBladeGen::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {		
	if (name == "SpinnerDeactivated") {	
//		Check if timed
		auto ResetTime = sender->GetVar<int32_t>(u"reset_time");
		if (ResetTime >= 1 && self->GetVar<int>(u"ToggleSpinner") != 1) {	
			self->AddTimer("Return", ResetTime + 2.5);	
//			2.5 = rough estimate of movetime for platforms		
		}
		if (self->GetVar<bool>(u"SpinnerIsUp")) {				
			SpinnerDescend(self);			
			self->CancelTimer("EntranceAnimLoop");				
		} else {
			SpinnerAscend(self);	
		}	
	}
}		

void SpinnerHighBladeGen::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "SkillPulse") {	
// 	Spinner damage skill exclusive for enemies	
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
	else if (timerName == "Return") {	
		if (self->GetVar<bool>(u"SpinnerIsUp")) {			
			SpinnerDescend(self);
		} else {
			SpinnerAscend(self);
		}	
	}
	else if (timerName == "EntranceAnimLoop") {			
		RenderComponent::PlayAnimation(self, u"idle-up");	
		if (self->GetVar<bool>(u"SpinnerIsUp")) {
			self->AddTimer("EntranceAnimLoop", 1);
		}	
	}
//	Handle blades GUID
	else if (timerName == "BladeGUID") {
		const auto AttachedPath = self->GetVar<std::u16string>(u"attached_path");
		if (AttachedPath == u"ZSpinner53") {
			GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{43d11dc0-a096-4595-8fbe-f95a4a0d951e}");	
		} else {
			GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{b1bbe65e-330d-4ef6-a534-63e98dd199ec}");	
		}
	}		
}
