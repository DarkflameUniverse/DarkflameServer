#include "FtMovingDoor4Statues.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "Entity.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"
#include "GameMessages.h"


void FtMovingDoor4Statues::OnStartup(Entity* self) {
	Progress = 0;
	
//	Additional safeguard for shared client script
	self->SetNetworkVar(u"bIsInUse", true);
	
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
	self->SetProximityRadius(360, "fetch_players");	
}	

void FtMovingDoor4Statues::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name == "fetch_players") {	
		if (entering->IsPlayer()) {
			
			if (!player1) {
				player1 = entering;
			} else if (!player2) {
				player2 = entering;
			} else if (!player3) {
				player3 = entering;
			} else if (!player4) {
				player4 = entering;
			}
		}	
    }	
	

}
	
void FtMovingDoor4Statues::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {

	if (args == "BuildComplete") {
		if (Progress == 3) {
			//Move Plat
			GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);

//			Completion check for boss
			const auto BossManager = Game::entityManager->GetEntitiesInGroup("BossManager");
			for (auto* bossobj : BossManager) {
				auto DoorValue = bossobj->GetVar<int>(u"DoorsOpen");
				bossobj->SetVar<int>(u"DoorsOpen", DoorValue + 1);	
			}

			//Distribute cinematics & audio
			//Their current musiccue state is unknown beyond the room
			const auto& cine = u"4StatuesComplete";	
			auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();						
			self->SetNetworkVar(u"flourishready", 1);

			
			self->AddTimer("PlayerFlourish", 2.5f);
			if (player1 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player1->GetObjectID())) {
				GameMessages::SendPlayCinematic(player1->GetObjectID(), cine, player1->GetSystemAddress());			

			}
			if (player2 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player2->GetObjectID())) {			
				GameMessages::SendPlayCinematic(player2->GetObjectID(), cine, player2->GetSystemAddress());	

			}
			if (player3 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player3->GetObjectID())) {		
				GameMessages::SendPlayCinematic(player3->GetObjectID(), cine, player3->GetSystemAddress());	
	
			}
			if (player4 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player4->GetObjectID())) {				
				GameMessages::SendPlayCinematic(player4->GetObjectID(), cine, player4->GetSystemAddress());	
	
			}			
			//end
			
		} else {
			Progress++;
		}
	}	
}

void FtMovingDoor4Statues::OnTimerDone(Entity* self, std::string timerName) {
	

	if (timerName == "PlayerFlourish") {	
		self->SetNetworkVar(u"triggermusic", 1);		
	}	
	
}

// Initialize defaults
Entity* FtMovingDoor4Statues::player1 = nullptr;
Entity* FtMovingDoor4Statues::player2 = nullptr;
Entity* FtMovingDoor4Statues::player3 = nullptr;
Entity* FtMovingDoor4Statues::player4 = nullptr;
