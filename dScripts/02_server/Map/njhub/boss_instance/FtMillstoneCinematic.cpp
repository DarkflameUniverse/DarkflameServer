// 32118
#include "FtMillstoneCinematic.h"
#include "MovingPlatformComponent.h"
#include "GameMessages.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "Entity.h"

// Initialize defaults
Entity* FtMillstoneCinematic::player1 = nullptr;
Entity* FtMillstoneCinematic::player2 = nullptr;
Entity* FtMillstoneCinematic::player3 = nullptr;
Entity* FtMillstoneCinematic::player4 = nullptr;

void FtMillstoneCinematic::OnStartup(Entity* self) {	
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
	self->SetProximityRadius(12, "CineRange");	
}

void FtMillstoneCinematic::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name == "CineRange") {
        if (entering->IsPlayer()) {
            const auto& cine = u"MillStoneIntro";
            

            if (entering != player1 && entering != player2 && entering != player3 && entering != player4) {
                if (!player1) {
                    player1 = entering;
                    GameMessages::SendPlayCinematic(player1->GetObjectID(), cine, player1->GetSystemAddress());
                } else if (!player2) {
                    player2 = entering;
                    GameMessages::SendPlayCinematic(player2->GetObjectID(), cine, player2->GetSystemAddress());
                } else if (!player3) {
                    player3 = entering;
                    GameMessages::SendPlayCinematic(player3->GetObjectID(), cine, player3->GetSystemAddress());			
                } else if (!player4) {
                    player4 = entering;
                    GameMessages::SendPlayCinematic(player4->GetObjectID(), cine, player4->GetSystemAddress());				
                }
            }
        }
	}
}



void FtMillstoneCinematic::OnTimerDone(Entity* self, std::string timerName) {
	
	
}


