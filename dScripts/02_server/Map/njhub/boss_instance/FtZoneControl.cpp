#include "FtZoneControl.h"
#include "EntityManager.h"
#include "Character.h"
#include "Entity.h"
#include "GameMessages.h"
#include "dZoneManager.h"
#include "RenderComponent.h"
#include "DestroyableComponent.h"
#include "TeamManager.h"
#include "eStateChangeType.h"


int PlayerCount = 0;
int BossPlayerCount = 0;

Entity* FtZoneControl::player1 = nullptr;
Entity* FtZoneControl::player2 = nullptr;
Entity* FtZoneControl::player3 = nullptr;
Entity* FtZoneControl::player4 = nullptr;

void FtZoneControl::OnStartup(Entity* self) {
	
	
	
}

void FtZoneControl::OnZoneLoadedInfo(Entity* self) {


	
}

void FtZoneControl::OnPlayerLoaded(Entity* self, Entity* player) {
	
//	Player spawns
//	Not used

	NiPoint3 pos1{};
	pos1.SetX(397.5);  
	pos1.SetY(281.14);  
	pos1.SetZ(146.5);  		
		
	NiPoint3 pos2{};
	pos2.SetX(389.5);  
	pos2.SetY(281.14);  
	pos2.SetZ(153.5);  	

	NiPoint3 pos3{};
	pos3.SetX(397.5);  
	pos3.SetY(281.14);  
	pos3.SetZ(163.5); 	

	NiPoint3 pos4{};
	pos4.SetX(389.5);  
	pos4.SetY(281.14);  
	pos4.SetZ(170.5);  		
		
	
	
	
//	Fill stats
	auto* destroyableComponent = player->GetComponent<DestroyableComponent>();
	if (destroyableComponent != nullptr) {
		destroyableComponent->SetHealth(static_cast<int32_t>(destroyableComponent->GetMaxHealth()));
		destroyableComponent->SetArmor(static_cast<int32_t>(destroyableComponent->GetMaxArmor()));
		destroyableComponent->SetImagination(static_cast<int32_t>(destroyableComponent->GetMaxImagination()));
	}	
	
	
    if (player != player1 && player != player2 && player != player3 && player != player4) {
        if (!player1) {
            player1 = player;
			PlayerCount++;
			BossPlayerCount++;
        } else if (!player2) {
            player2 = player;
			PlayerCount++;
			BossPlayerCount++;			
        } else if (!player3) {
            player3 = player;	
			PlayerCount++;
			BossPlayerCount++;			
        } else if (!player4) {
            player4 = player;
			PlayerCount++;
			BossPlayerCount++;			
        }
		
//		Send team size		

		const auto GarmadonChest = Game::entityManager->GetEntitiesByLOT(16842);
		const auto WavesManager = Game::entityManager->GetEntitiesInGroup("WavesManager");
		const auto BossManager = Game::entityManager->GetEntitiesInGroup("BossManager");
		for (auto* wavesobj : WavesManager) {
			wavesobj->SetVar(u"playercount", PlayerCount);	
		}
		for (auto* bossobj : BossManager) {
			bossobj->SetVar<int>(u"playercount", BossPlayerCount);	
		}		
		for (auto* chest : GarmadonChest) {
			chest->SetVar<int>(u"playercount", BossPlayerCount);
		}	
    }	


//	Tell client when players loaded


	self->AddCallbackTimer(0.0f, [self, player, this]() {				
//		self->AddTimer("PlayerLoaded", 1);

		if (self->GetNetworkVar<int>(u"playersloaded") == 1) {
			self->SetNetworkVar<int>(u"playersloaded", 0);	
		} else {
			self->SetNetworkVar<int>(u"playersloaded", 1);	
		}	
	});

}

void FtZoneControl::OnPlayerExit(Entity* self, Entity* player) {	
		
	
	PlayerCount--;
	BossPlayerCount--;
	
	const auto GarmadonChest = Game::entityManager->GetEntitiesByLOT(16842);
	const auto WavesManager = Game::entityManager->GetEntitiesInGroup("WavesManager");
	const auto BossManager = Game::entityManager->GetEntitiesInGroup("BossManager");
	for (auto* wavesobj : WavesManager) {
		wavesobj->SetVar(u"playercount", PlayerCount);	
	}
	for (auto* bossobj : BossManager) {
		bossobj->SetVar<int>(u"playercount", BossPlayerCount);	
	}		
	for (auto* chest : GarmadonChest) {
		chest->SetVar<int>(u"playercount", BossPlayerCount);	
	}	
}

void FtZoneControl::OnTimerDone(Entity* self, std::string timerName) {
		
	if (timerName == "PlayerLoaded") {	
	

		self->SetNetworkVar<int>(u"playersloaded", 1);	

	} 
}
