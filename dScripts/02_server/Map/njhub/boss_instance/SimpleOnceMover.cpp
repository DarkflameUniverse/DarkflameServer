//////////
//////////////////////
// General script for moving pillars & generic spinners


#include "SimpleOnceMover.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

void SimpleOnceMover::OnStartup(Entity* self) {
	if (self->GetVar<std::u16string>(u"toggle") == u"toggle_activate") {
		self->SetVar<int>(u"ToggleSpinner", 1);	
	} 	
}

void SimpleOnceMover::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {
	const auto AttachedPath = self->GetVar<std::u16string>(u"attached_path");
	auto ResetTime = sender->GetVar<int32_t>(u"reset_time");	
	if (name == "SpinnerDeactivated") {		
		if (self->GetVar<int>(u"ToggleSpinner") == 1) {
			RenderComponent::PlayAnimation(self, u"up");
			GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
			
	//		Ascend sfx
			GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{5c30c263-00ae-42a2-80a3-2ae33c8f13fe}");	
			self->AddTimer("AscentGUID", 0.1f);	

			self->SetVar<int>(u"ToggleSpinner", 2);		
			
		} else if (self->GetVar<int>(u"ToggleSpinner") == 2) {
			self->AddTimer("MoveDown", 0.1f);									
			self->SetVar<int>(u"ToggleSpinner", 1);	
	
		} else if (ResetTime >= 1) {	
			RenderComponent::PlayAnimation(self, u"up");
			GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);			
	//		Ascend sfx
			GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{5c30c263-00ae-42a2-80a3-2ae33c8f13fe}");	
			
			self->AddTimer("AscentGUID", 0.1f);	
			self->AddTimer("MoveDown", ResetTime + 2.5);	
				
		} else {
			GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);	
			if (AttachedPath == u"Spinner1") {
				self->AddTimer("PlayAnim", 0.2f);			
			} 
		} 
	}
}

void SimpleOnceMover::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "PlayAnim") {	
		RenderComponent::PlayAnimation(self, u"up");
		
//		Ascend sfx
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{5c30c263-00ae-42a2-80a3-2ae33c8f13fe}");	
		self->AddTimer("AscentGUID", 0.1f);
	} 
	else if (timerName == "MoveDown") {	
		RenderComponent::PlayAnimation(self, u"down");
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1);
			
//		Descend sfx
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{40e86d71-084c-4149-884e-ab9b45b694dc}");	
		self->AddTimer("DescentGUID", 0.1f);			
	}
//	Handle spinner sound orders
	else if (timerName == "AscentGUID") {
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");	
	}
	else if (timerName == "DescentGUID") {
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");	
	}	
}
