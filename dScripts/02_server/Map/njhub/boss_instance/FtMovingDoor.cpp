#include "FtMovingDoor.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

void FtMovingDoor::OnStartup(Entity* self) {
	self->SetVar<int>(u"SpinnerProgress", 0);	
}

void FtMovingDoor::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {
	const auto AttachedPath = self->GetVar<std::u16string>(u"attached_path");
	auto Progress = self->GetVar<int>(u"SpinnerProgress");
	
	if (name == "SpinnerDeactivated") {	
		if (AttachedPath == u"ZSpinner78") {
			if (Progress == 7) {
				self->AddTimer("MoveUp", 5.4f);
				DoorsProgress(self);
			}
			self->SetVar<int>(u"SpinnerProgress", Progress + 1);	
		} else {
			GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
			DoorsProgress(self);
		}	
	} else if (name == "WavesSpinnerDeactivated") {		
		self->AddTimer("MoveUp", 1.1f);	
		DoorsProgress(self);
	}
}

void FtMovingDoor::DoorsProgress(Entity* self) {
//	Completion check for boss
	const auto BossManager = Game::entityManager->GetEntitiesInGroup("BossManager");
	for (auto* bossobj : BossManager) {
		auto DoorValue = bossobj->GetVar<int>(u"DoorsOpen");
		bossobj->SetVar<int>(u"DoorsOpen", DoorValue + 1);	
	}	
}	

void FtMovingDoor::OnTimerDone(Entity* self, std::string timerName) {	
	if (timerName == "MoveUp") {	
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
	}
}		
