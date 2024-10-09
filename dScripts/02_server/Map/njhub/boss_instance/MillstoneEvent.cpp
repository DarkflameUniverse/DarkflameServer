//Used for LOT 32050 to not forget

#include "MillstoneEvent.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

void MillstoneEvent::OnStartup(Entity* self) {
	self->SetNetworkVar(u"bIsInUse", false);
	self->SetVar(u"bActive", true);

	self->SetVar<int>(u"ZCurrentWaypoint", 0);
	self->SetVar<int>(u"ZNextWaypoint", 1);
		
//	force waypoint over pathing garbage		
	self->AddTimer("SetWaypoint", 29);	
}

void MillstoneEvent::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {
	const auto AttachedPath = self->GetVar<std::u16string>(u"attached_path");	
	if (name == "SpinnerDeactivated") {	

		if (AttachedPath == u"MillstoneA") {
			self->AddTimer("MoveUp", 0.1f);			
		return;
		}
			
		auto CurrentWaypoint = self->GetVar<int>(u"ZCurrentWaypoint");	
		auto NextWaypoint = self->GetVar<int>(u"ZNextWaypoint");		
		
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, CurrentWaypoint, NextWaypoint, 
		NextWaypoint);
		self->SetVar<int>(u"ZCurrentWaypoint", NextWaypoint);
		if (NextWaypoint != 2 && AttachedPath == u"MillstoneB") {
			self->SetVar<int>(u"ZNextWaypoint", NextWaypoint + 1);			
		} else if (NextWaypoint != 3 && AttachedPath == u"MillstoneC") {
			self->SetVar<int>(u"ZNextWaypoint", NextWaypoint + 1);				
		} else {	
			self->SetVar<int>(u"ZNextWaypoint", 0);				
		}
	}	
}		

void MillstoneEvent::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "SetWaypoint") {	
//		z_CurrentWaypoint = 0;	
//		z_NextWaypoint = 1;	
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
	}	
	
//	Loop sequence for MillstoneA	
	else if (timerName == "MoveUp") {	
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1);	
		self->AddTimer("MoveDown", 9.0f);			
	} else if (timerName == "MoveDown") {	
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
		self->AddTimer("MoveUp", 9.0f);			
	}	
}
