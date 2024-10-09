//	Since everything had to be timed right & the script's structure wasn't planned as usual,
//	all the code got funneled into the timers
//	goodluck trying to clean this :P




#include "FtWavesManager.h"
#include "GameMessages.h"
#include "ProximityMonitorComponent.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "RenderComponent.h"
#include "BuffComponent.h"
#include "BaseCombatAIComponent.h"

// Initialize defaults
Entity* FtWavesManager::engaged1 = nullptr;
Entity* FtWavesManager::engaged2 = nullptr;
Entity* FtWavesManager::engaged3 = nullptr;
Entity* FtWavesManager::engaged4 = nullptr;



void FtWavesManager::OnStartup(Entity* self) {
	
	
	Wave1Progress = 0;
	Wave2Progress = 0;
	Wave3Progress = 0;
	
	WaveNum = 1;	

	LargeGroup = 1;
	
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
	self->SetProximityRadius(4500, "Area_Radius");	
	self->SetProximityRadius(150, "Room_Radius");



	if (self->GetVar<int>(u"playercount") == 1) {
		LargeGroup = 0;
	}
}

void FtWavesManager::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {	
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();		
	if (name == "Area_Radius") {	
		if (entering->IsPlayer()) {
//		collect & store player entities for cines	
			if (entering != engaged1 && entering != engaged2 && entering != engaged3 && entering != engaged4) {	
				if (!engaged1) {
					engaged1 = entering;
					self->SetVar(u"playerid1", entering->GetObjectID());					
				} else if (!engaged2) {
					engaged2 = entering;	
					self->SetVar(u"playerid2", entering->GetObjectID());							
				} else if (!engaged3) {
					engaged3 = entering;
					self->SetVar(u"playerid3", entering->GetObjectID());							
				} else if (!engaged4) {
					engaged4 = entering;
					self->SetVar(u"playerid4", entering->GetObjectID());							
				}
			}	
		}
	}
}

void FtWavesManager::OnCollisionPhantom(Entity* self, Entity* target) {
		
	if (target->IsPlayer()) {
		
	

		if (bStarted == 1) {
			return;
		}	
		bStarted = 1;	

//		check team size again
		LargeGroup = 1;
		if (self->GetVar<int>(u"playercount") == 1) {
			LargeGroup = 0;
		}
//		end	
	
		ActivateWaveSpinners(self);	
	}	
}

void FtWavesManager::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
int32_t param3) {
	
	
}

void FtWavesManager::ActivateWaveSpinners(Entity* self) {

	if (WaveNum == 4) {
		return;
	}
			

//	Cinematic stuff		
    Entity* engagedEntities[] = {engaged1, engaged2, engaged3, engaged4};
	
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
    for (Entity* engaged : engagedEntities) {
        if (engaged != nullptr && proximityMonitorComponent->IsInProximity("Room_Radius", engaged->GetObjectID())
			&& WaveNum != 3) {
            auto* buffComponent = engaged->GetComponent<BuffComponent>();
            if (buffComponent != nullptr) {
                buffComponent->RemoveBuff(60);
            }

			GameMessages::SendPlayCinematic(engaged->GetObjectID(), u"WavesSpawnerSpinners", 
			engaged->GetSystemAddress());

		} else if (engaged != nullptr && proximityMonitorComponent->IsInProximity("Area_Radius", engaged->GetObjectID())
			&& WaveNum == 3) {
				
            auto* buffComponent = engaged->GetComponent<BuffComponent>();
            if (buffComponent != nullptr) {
                buffComponent->RemoveBuff(60);
            }

			GameMessages::SendPlayCinematic(engaged->GetObjectID(), u"Waves_Wave3Cam_1", 
			engaged->GetSystemAddress());	
			self->AddTimer("MusicOff", 1);		
		}
    }
//	End


	

//	Wave enemy spawning & spinner timers
	if (WaveNum == 1) {	
	
//	For client sounds
		self->SetNetworkVar(u"wave1", 1);		

//	Timings for spinners
		HandleSpinner(self, "hand", "up");		
		HandleSpinner(self, "blacksmith", "up");	
		self->AddTimer("SpinnersDown", 10);		
		

		self->AddTimer("Wave1LeftSpinners", 3);
		self->AddTimer("Wave1RightEnemies", 3.3f);
		
		self->AddTimer("UnstunEnemies", 8.5f);	
		
	} else if (WaveNum == 2) {

//	For client sounds
		self->SetNetworkVar(u"wave2", 1);	

//	Timings for spinners
		HandleSpinner(self, "hand", "up");		
		HandleSpinner(self, "scientist", "up");	
		self->AddTimer("Wave2LeftSpinners", 3);
		self->AddTimer("SpinnersDown", 10);			
		self->AddTimer("Wave2RightEnemies", 3.3f);	

		self->AddTimer("UnstunEnemies", 8.5f);		

	} else if (WaveNum == 3) {			
			self->AddTimer("FinalSpinnersChain1", 2.8f);
			
			self->AddTimer("GoldSpinnerMove", 12.3f);
			
			self->AddTimer("SpinnersDownChain1", 20);	
			
			self->AddTimer("FinalEnemies", 12.5f);				
			
			self->AddTimer("UnstunEnemies", 26);		
	}
}

void FtWavesManager::HandleSpinner(Entity* self, std::string spinner, std::string direction) {	
//	Get spinner entity
	auto BlacksmithSpinner = Game::entityManager->GetEntitiesInGroup("WavesSpinnerBlacksmith");
	auto HandSpinner = Game::entityManager->GetEntitiesInGroup("WavesSpinnerHand");	
	auto OverseerSpinner = Game::entityManager->GetEntitiesInGroup("WavesSpinnerOverseer");	
	auto MarksmanSpinner = Game::entityManager->GetEntitiesInGroup("WavesSpinnerMarksman");	
	auto WolfSpinner = Game::entityManager->GetEntitiesInGroup("WavesSpinnerWolf");	
	auto BeetleSpinner = Game::entityManager->GetEntitiesInGroup("WavesSpinnerBeetle");	
	auto ScientistSpinner = Game::entityManager->GetEntitiesInGroup("WavesSpinnerScientist");	
	auto BonezaiSpinner = Game::entityManager->GetEntitiesInGroup("WavesSpinnerBonezai");	
	auto GoldSpinner = Game::entityManager->GetEntitiesInGroup("WavesElevatorSpinner");		
	
    auto ProcessSpinnerUp = [](const std::vector<Entity*>& spinnerGroup) {
        for (auto* spinner : spinnerGroup) {
            GameMessages::SendPlatformResync(spinner, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
            RenderComponent::PlayAnimation(spinner, u"up");
            GameMessages::SendPlayNDAudioEmitter(spinner, spinner->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");
            GameMessages::SendPlayFXEffect(spinner->GetObjectID(), 10102, u"create", "create");
        }
    };
    auto ProcessSpinnerDown = [](const std::vector<Entity*>& spinnerGroup) {
        for (auto* spinner : spinnerGroup) {
            GameMessages::SendPlatformResync(spinner, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1, 1, eMovementPlatformState::Moving);
            RenderComponent::PlayAnimation(spinner, u"down");
            GameMessages::SendPlayNDAudioEmitter(spinner, spinner->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");

        }
    };

    if (direction == "up") {
		
        if (spinner == "hand") {
			HandSpinnerUp = 1;
            ProcessSpinnerUp(HandSpinner);
        } else if (spinner == "blacksmith") {
			BlacksmithSpinnerUp = 1;		
            ProcessSpinnerUp(BlacksmithSpinner);
        } else if (spinner == "overseer") {
			OverseerSpinnerUp = 1;			
            ProcessSpinnerUp(OverseerSpinner);
        } else if (spinner == "marksman") {
			MarksmanSpinnerUp = 1;			
            ProcessSpinnerUp(MarksmanSpinner);
        } else if (spinner == "wolf") {
			WolfSpinnerUp = 1;		
            ProcessSpinnerUp(WolfSpinner);
        } else if (spinner == "beetle") {
			BeetleSpinnerUp = 1;		
            ProcessSpinnerUp(BeetleSpinner);
        } else if (spinner == "scientist") {
			ScientistSpinnerUp = 1;		
            ProcessSpinnerUp(ScientistSpinner);
        } else if (spinner == "bonezai") {
			BonezaiSpinnerUp = 1;			
            ProcessSpinnerUp(BonezaiSpinner);
        } 	
		
	} else if (direction == "down") {	
	
        if (spinner == "hand") {
			HandSpinnerUp = 0;
            ProcessSpinnerDown(HandSpinner);
        } else if (spinner == "blacksmith") {
			BlacksmithSpinnerUp = 0;			
            ProcessSpinnerDown(BlacksmithSpinner);
        } else if (spinner == "overseer") {
			OverseerSpinnerUp = 0;			
            ProcessSpinnerDown(OverseerSpinner);
        } else if (spinner == "marksman") {
			MarksmanSpinnerUp = 0;			
            ProcessSpinnerDown(MarksmanSpinner);
        } else if (spinner == "wolf") {
			WolfSpinnerUp = 0;		
            ProcessSpinnerDown(WolfSpinner);
        } else if (spinner == "beetle") {
			BeetleSpinnerUp = 0;	
            ProcessSpinnerDown(BeetleSpinner);
        } else if (spinner == "scientist") {
			ScientistSpinnerUp = 0;		
            ProcessSpinnerDown(ScientistSpinner);
        } else if (spinner == "bonezai") {
			BonezaiSpinnerUp = 0;			
            ProcessSpinnerDown(BonezaiSpinner);
        } else if (spinner == "gold") {
//			Path is flipped handle manually			
			for (auto* spinner : GoldSpinner) {
				GameMessages::SendPlatformResync(spinner, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
				RenderComponent::PlayAnimation(spinner, u"down");
				GameMessages::SendPlayNDAudioEmitter(spinner, spinner->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");
			}		
		}
	}

}

void FtWavesManager::CheckWaveProgress(Entity* self) {

//	Wave 1	
	if (LargeGroup == 1 && Wave1Progress == 7) {
		Wave1Progress = 9;			
		self->AddTimer("ShowDoor", 1);		
	}
	if (LargeGroup == 0 && Wave1Progress == 4) {
		Wave1Progress = 9;			
		self->AddTimer("ShowDoor", 1);	
	}	

//	Wave 2
	if (LargeGroup == 1 && Wave2Progress == 10) {
		Wave2Progress = 12;			
		self->AddTimer("ShowDoor", 1);		
	}
	if (LargeGroup == 0 && Wave2Progress == 6) {
		Wave2Progress = 12;			
		self->AddTimer("ShowDoor", 1);	
	}		
	
//	Wave 3

	
	if (LargeGroup == 1 && Wave3Progress == 14) {
		Wave3Progress = 17;			
		self->AddTimer("DoorActive", 7);	
		
		self->AddTimer("SpawnBouncer", 12);			

//		Give players extra time to collect loot
		self->AddTimer("ShowDoor", 5);		


		self->SetNetworkVar(u"wave4", 1);		

//		Clear poison while collecting loot	
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
		Entity* engagedEntities[] = {engaged1, engaged2, engaged3, engaged4};

		for (Entity* engaged : engagedEntities) {
			if (engaged != nullptr && proximityMonitorComponent->IsInProximity("Room_Radius", engaged->GetObjectID())) {
				auto* buffComponent = engaged->GetComponent<BuffComponent>();
				if (buffComponent != nullptr) {
					buffComponent->RemoveBuff(60);
				}	
			}
		}	
	}
	if (LargeGroup == 0 && Wave3Progress == 8) {
		Wave3Progress = 16;		
		self->AddTimer("DoorActive", 7);	
		
		self->AddTimer("SpawnBouncer", 12);		
		
		self->AddTimer("ShowDoor", 5);		
		
		
		self->SetNetworkVar(u"wave4", 1);			
		
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
		Entity* engagedEntities[] = {engaged1, engaged2, engaged3, engaged4};

		for (Entity* engaged : engagedEntities) {
			if (engaged != nullptr && proximityMonitorComponent->IsInProximity("Room_Radius", engaged->GetObjectID())) {
				auto* buffComponent = engaged->GetComponent<BuffComponent>();
				if (buffComponent != nullptr) {
					buffComponent->RemoveBuff(60);
				}	
			}
		}
	}	

}	


void FtWavesManager::OnTimerDone(Entity* self, std::string timerName) {
	Entity* playerEntities[] = {engaged1, engaged2, engaged3, engaged4};	
	
	if (timerName == "ShowDoor") {
		
//		More cinematic stuff
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
		Entity* engagedEntities[] = {engaged1, engaged2, engaged3, engaged4};

		for (Entity* engaged : engagedEntities) {
			if (engaged != nullptr && proximityMonitorComponent->IsInProximity("Room_Radius", engaged->GetObjectID())) {
				auto* buffComponent = engaged->GetComponent<BuffComponent>();
				if (buffComponent != nullptr) {
					buffComponent->RemoveBuff(60);
				}
	
				if (WaveNum == 1) {
					GameMessages::SendPlayCinematic(engaged->GetObjectID(), u"WavesDoorCam_Torch01", engaged->GetSystemAddress());
				} else if (WaveNum == 2) {				
					GameMessages::SendPlayCinematic(engaged->GetObjectID(), u"WavesDoorCam_Torch02", engaged->GetSystemAddress());
				} else if (WaveNum == 3) {				
					GameMessages::SendPlayCinematic(engaged->GetObjectID(), u"WavesDoorCam_Torch03", engaged->GetSystemAddress());					
				}
			}
		}
//		End


		
		self->AddTimer("TurnOnIndicator", 1);

		self->AddTimer("SpawnNextWave", 14);
		
//		Timing for large group final wave audio		
		if (WaveNum == 2) {
			self->AddTimer("MusicOff", 15);
			self->AddTimer("FinalWaveLargeAudio", 10.7f);	
		} else if (WaveNum == 3) {
			self->SetNetworkVar(u"wave4cine", 1);
		}
	
	} else if (timerName == "TurnOnIndicator") {
//	Get proper door indicator		
		if (WaveNum == 1) {	
			auto DoorIndicator = Game::entityManager->GetEntitiesInGroup("WavesDoorIndicator1");				
			for (auto* indicator : DoorIndicator) {
				if (indicator) {	
					indicator->SetNetworkVar<bool>(u"FlameOn", true);	
				}
			}		
			WaveNum = 2;			
		} else if (WaveNum == 2) {	
			WaveNum = 3;			
			auto DoorIndicator = Game::entityManager->GetEntitiesInGroup("WavesDoorIndicator2");				
			for (auto* indicator : DoorIndicator) {
				if (indicator) {	
					indicator->SetNetworkVar<bool>(u"FlameOn", true);	
				}
			}							
		} else if (WaveNum == 3) {			
			auto DoorIndicator = Game::entityManager->GetEntitiesInGroup("WavesDoorIndicator3");				
			for (auto* indicator : DoorIndicator) {
				if (indicator) {	
					indicator->SetNetworkVar<bool>(u"FlameOn", true);	
				}
			}	
			WaveNum = 4;		
		}
	} else if (timerName == "SpawnNextWave") {
		ActivateWaveSpinners(self);			
	
	} else if (timerName == "SpinnersDown") {
		if (HandSpinnerUp == 1) {	
			HandleSpinner(self, "hand", "down");			
		} if (BlacksmithSpinnerUp == 1) {
			HandleSpinner(self, "blacksmith", "down");			
		} if (OverseerSpinnerUp == 1) {
			HandleSpinner(self, "overseer", "down");				
		} if (MarksmanSpinnerUp == 1) {
			HandleSpinner(self, "marksman", "down");	
		} if (WolfSpinnerUp == 1) {
			HandleSpinner(self, "wolf", "down");	
		} if (BeetleSpinnerUp == 1) {
			HandleSpinner(self, "beetle", "down");	
		} if (ScientistSpinnerUp == 1) {
			HandleSpinner(self, "scientist", "down");	
		} if (BonezaiSpinnerUp == 1) {
			HandleSpinner(self, "bonezai", "down");				
		}	

	} else if (timerName == "Wave1LeftSpinners") {
		self->AddTimer("Wave1LeftEnemies", 3.3f);
		HandleSpinner(self, "marksman", "up");

	} else if (timerName == "Wave2LeftSpinners") {
		self->AddTimer("Wave2LeftEnemies", 3.3f);
		HandleSpinner(self, "wolf", "up");
		HandleSpinner(self, "beetle", "up");
	
	
	} else if (timerName == "Wave1RightEnemies") {
		
		auto blacksmithspawners = Game::zoneManager->GetSpawnersByName("Waves_Blacksmith_Spawner");
		auto handspawners = Game::zoneManager->GetSpawnersByName("Waves_Hand_Spawner");		
		auto* blacksmithspawner = blacksmithspawners.front();
		auto* handspawner = handspawners.front();		
		blacksmithspawner->SetSpawnLot(32335);	
		handspawner->SetSpawnLot(14001);		
		
		if (LargeGroup == 1) {	
			for (auto i = 0; i < 3; i++) {
				blacksmithspawner->Spawn({ blacksmithspawner->m_Info.nodes.at(i % blacksmithspawner->m_Info.nodes.size()) }, true);
			}
			for (auto i = 0; i < 2; i++) {
				handspawner->Spawn({ handspawner->m_Info.nodes.at(i % handspawner->m_Info.nodes.size()) }, true);
			}
		} else if (LargeGroup == 0) {	
			for (auto i = 0; i < 2; i++) {
				blacksmithspawner->Spawn({ blacksmithspawner->m_Info.nodes.at(i % blacksmithspawner->m_Info.nodes.size()) }, true);
			}
			for (auto i = 0; i < 1; i++) {
				handspawner->Spawn({ handspawner->m_Info.nodes.at(i % handspawner->m_Info.nodes.size()) }, true);
			}
		}

		
		auto blacksmithEntities = Game::entityManager->GetEntitiesByLOT(32335);	
		auto handEntities = Game::entityManager->GetEntitiesByLOT(14001);		
		
		for (auto* hand : handEntities) {
			auto* CombatAI = hand->GetComponent<BaseCombatAIComponent>();
			if (CombatAI != nullptr) {
				CombatAI->SetDisabled(true);
			}			
			hand->AddDieCallback([self, this]() {	
				Wave1Progress++;	
				CheckWaveProgress(self);					
			});	
		}
		for (auto* blacksmith : blacksmithEntities) {
			auto* CombatAI = blacksmith->GetComponent<BaseCombatAIComponent>();
			if (CombatAI != nullptr) {
				CombatAI->SetDisabled(true);
			}	
			blacksmith->AddDieCallback([self, this]() {	
				Wave1Progress++;
				CheckWaveProgress(self);	
			});	
		}			
	} else if (timerName == "Wave2RightEnemies") {
		
		auto handspawners = Game::zoneManager->GetSpawnersByName("Waves_Hand_Spawner");	
		auto scientistspawners = Game::zoneManager->GetSpawnersByName("Waves_Scientist_Spawner");	
		auto* handspawner = handspawners.front();
		auto* scientistspawner = scientistspawners.front();	
		handspawner->SetSpawnLot(14001);
		scientistspawner->SetSpawnLot(16938);		
		
		if (LargeGroup == 1) {			
			for (auto i = 0; i < 4; i++) {
				scientistspawner->Spawn({ scientistspawner->m_Info.nodes.at(i % scientistspawner->m_Info.nodes.size()) }, true);
			}
			for (auto i = 0; i < 2; i++) {
				handspawner->Spawn({ handspawner->m_Info.nodes.at(i % handspawner->m_Info.nodes.size()) }, true);
			}
		} else if (LargeGroup == 0) {	
			for (auto i = 0; i < 2; i++) {
				handspawner->Spawn({ handspawner->m_Info.nodes.at(i % handspawner->m_Info.nodes.size()) }, true);				
				scientistspawner->Spawn({ scientistspawner->m_Info.nodes.at(i % scientistspawner->m_Info.nodes.size()) }, true);
			}
		}

		auto handEntities = Game::entityManager->GetEntitiesByLOT(14001);
		auto scientistEntities = Game::entityManager->GetEntitiesByLOT(16938);				
		
		for (auto* hand : handEntities) {
			auto* CombatAI = hand->GetComponent<BaseCombatAIComponent>();
			if (CombatAI != nullptr) {
				CombatAI->SetDisabled(true);
			}	
			hand->AddDieCallback([self, this]() {	
				Wave2Progress++;	
				CheckWaveProgress(self);					
			});	
		}
		for (auto* scientist : scientistEntities) {
			auto* CombatAI = scientist->GetComponent<BaseCombatAIComponent>();
			if (CombatAI != nullptr) {
				CombatAI->SetDisabled(true);
			}	
			scientist->AddDieCallback([self, this]() {	
				Wave2Progress++;
				CheckWaveProgress(self);	
			});	
		}					
	
	} else if (timerName == "Wave1LeftEnemies") {
		
		auto marksmanspawners = Game::zoneManager->GetSpawnersByName("Waves_Marksman_Spawner");
		auto* marksmanspawner = marksmanspawners.front();	
		marksmanspawner->SetSpawnLot(16848);
		
		if (LargeGroup == 1) {	
			for (auto i = 0; i < 2; i++) {
				marksmanspawner->Spawn({ marksmanspawner->m_Info.nodes.at(i % marksmanspawner->m_Info.nodes.size()) }, true);
			}
		} else if (LargeGroup == 0) {	
			for (auto i = 0; i < 1; i++) {
				marksmanspawner->Spawn({ marksmanspawner->m_Info.nodes.at(i % marksmanspawner->m_Info.nodes.size()) }, true);
			}
		}

		auto marksmanEntities = Game::entityManager->GetEntitiesByLOT(16848);
		
		for (auto* marksman : marksmanEntities) {
			auto* CombatAI = marksman->GetComponent<BaseCombatAIComponent>();
			if (CombatAI != nullptr) {
				CombatAI->SetDisabled(true);
			}	
			marksman->AddDieCallback([self, this]() {	
				Wave1Progress++;
				CheckWaveProgress(self);				
			});	
		}
		
	} else if (timerName == "Wave2LeftEnemies") {
		
		auto wolfspawners = Game::zoneManager->GetSpawnersByName("Waves_Wolf_Spawner");
		auto beetlespawners = Game::zoneManager->GetSpawnersByName("Waves_Beetle_Spawner");		
		auto* wolfspawner = wolfspawners.front();
		auto* beetlespawner = beetlespawners.front();
		
		wolfspawner->SetSpawnLot(16850);
		beetlespawner->SetSpawnLot(32336);		
		
		if (LargeGroup == 1) {			
			for (auto i = 0; i < 2; i++) {
				wolfspawner->Spawn({ wolfspawner->m_Info.nodes.at(i % wolfspawner->m_Info.nodes.size()) }, true);
				beetlespawner->Spawn({ beetlespawner->m_Info.nodes.at(i % beetlespawner->m_Info.nodes.size()) }, true);
			}
		} else if (LargeGroup == 0) {	
			for (auto i = 0; i < 1; i++) {
				wolfspawner->Spawn({ wolfspawner->m_Info.nodes.at(i % wolfspawner->m_Info.nodes.size()) }, true);
				beetlespawner->Spawn({ beetlespawner->m_Info.nodes.at(i % beetlespawner->m_Info.nodes.size()) }, true);
			}
		}

		auto beetleEntities = Game::entityManager->GetEntitiesByLOT(32336);		
		auto wolfEntities = Game::entityManager->GetEntitiesByLOT(16850);			
		

		for (auto* beetle : beetleEntities) {
			auto* CombatAI = beetle->GetComponent<BaseCombatAIComponent>();
			if (CombatAI != nullptr) {
				CombatAI->SetDisabled(true);
			}	
			beetle->AddDieCallback([self, this]() {	
				Wave2Progress++;
				CheckWaveProgress(self);				
			});	
		}	
		for (auto* wolf : wolfEntities) {
			auto* CombatAI = wolf->GetComponent<BaseCombatAIComponent>();
			if (CombatAI != nullptr) {
				CombatAI->SetDisabled(true);
			}	
			wolf->AddDieCallback([self, this]() {	
				Wave2Progress++;
				CheckWaveProgress(self);	
			});	
		}	
		
	} else if (timerName == "UnstunEnemies") {
		struct {
			uint32_t LOT;
			std::vector<Entity*> entities;
		} entityGroups[] = {
			{ 16938, Game::entityManager->GetEntitiesByLOT(16938) },   // scientistEntities
			{ 16850, Game::entityManager->GetEntitiesByLOT(16850) },   // wolfEntities
			{ 32336, Game::entityManager->GetEntitiesByLOT(32336) },   // beetleEntities
			{ 16853, Game::entityManager->GetEntitiesByLOT(16853) },   // bonezaiEntities
			{ 16846, Game::entityManager->GetEntitiesByLOT(16846) },   // overseerEntities
			{ 16848, Game::entityManager->GetEntitiesByLOT(16848) },   // marksmanEntities
			{ 14001, Game::entityManager->GetEntitiesByLOT(14001) },   // handEntities
			{ 32335, Game::entityManager->GetEntitiesByLOT(32335) }    // blacksmithEntities
		};

		for (auto& group : entityGroups) {
			for (auto* entity : group.entities) {
				auto* CombatAI = entity->GetComponent<BaseCombatAIComponent>();
				if (CombatAI != nullptr) {
					CombatAI->SetDisabled(false);
				}
			}
		}
	} else if (timerName == "GoldSpinnerMove") {
		
		HandleSpinner(self, "gold", "down");		
		
	} else if (timerName == "SmallFinalSpinnersUp") {		
		HandleSpinner(self, "hand", "up");
		HandleSpinner(self, "wolf", "up");	
		HandleSpinner(self, "beetle", "up");
		HandleSpinner(self, "bonezai", "up");			
		HandleSpinner(self, "marksman", "up");	
		HandleSpinner(self, "overseer", "up");	
		HandleSpinner(self, "scientist", "up");			
		HandleSpinner(self, "blacksmith", "up");
		
	} else if (timerName == "SpinnersDownChain1") {				
		if (MarksmanSpinnerUp == 1) {
			HandleSpinner(self, "marksman", "down");		
		} if (BonezaiSpinnerUp == 1) {
			HandleSpinner(self, "bonezai", "down");				
		}		
		self->AddTimer("SpinnersDownChain2", 1.1f);		
	} else if (timerName == "SpinnersDownChain2") {
		if (HandSpinnerUp == 1) {	
			HandleSpinner(self, "hand", "down");			
		} if (BeetleSpinnerUp == 1) {
			HandleSpinner(self, "beetle", "down");	
		}
		self->AddTimer("SpinnersDownChain3", 1.1f);		
	} else if (timerName == "SpinnersDownChain3") {
		if (WolfSpinnerUp == 1) {
			HandleSpinner(self, "wolf", "down");	
		} if (ScientistSpinnerUp == 1) {
			HandleSpinner(self, "scientist", "down");			
		}
		self->AddTimer("SpinnersDownChain4", 1.1f);				
	} else if (timerName == "SpinnersDownChain4") {		
		if (BlacksmithSpinnerUp == 1) {
			HandleSpinner(self, "blacksmith", "down");			
		} if (OverseerSpinnerUp == 1) {
			HandleSpinner(self, "overseer", "down");				
		}
	} else if (timerName == "FinalEnemies") {	
		std::vector<std::pair<std::vector<Spawner*>, int>> spawnerData = {
			{ Game::zoneManager->GetSpawnersByName("Waves_Hand_Spawner"), 14001 },			
			{ Game::zoneManager->GetSpawnersByName("Waves_Blacksmith_Spawner"), 32335 },
			{ Game::zoneManager->GetSpawnersByName("Waves_Marksman_Spawner"), 16848 },
			{ Game::zoneManager->GetSpawnersByName("Waves_Scientist_Spawner"), 16938 },
			{ Game::zoneManager->GetSpawnersByName("Waves_Wolf_Spawner"), 16850 },
			{ Game::zoneManager->GetSpawnersByName("Waves_Overseer_Spawner"), 16846 },			
			{ Game::zoneManager->GetSpawnersByName("Waves_Beetle_Spawner"), 32336 },
			{ Game::zoneManager->GetSpawnersByName("Waves_Bonezai_Spawner"), 16853 }
		};

		std::vector<Spawner*> spawners;

		for (auto& data : spawnerData) {
			auto& spawnerList = data.first;
			int spawnLot = data.second;
			if (!spawnerList.empty()) {
				auto* spawner = spawnerList.front();
				spawner->SetSpawnLot(spawnLot);
				spawners.push_back(spawner);
			}
		}

		std::vector<int> spawnCountsSmallGroup = { 1, 1, 1, 1, 1, 1, 1, 1 };
		std::vector<int> spawnCountsLargeGroup = { 3, 2, 2, 2, 2, 1, 1, 1 };
		
		auto spawnEntities = [&](int count, int spawnerIndex) {
			for (auto i = 0; i < count; i++) {
				spawners[spawnerIndex]->Spawn({ spawners[spawnerIndex]->m_Info.nodes.at(i % spawners[spawnerIndex]->m_Info.nodes.size()) }, true);
			}
		};

		if (LargeGroup == 0) {
			for (size_t i = 0; i < spawnCountsSmallGroup.size(); i++) {
				spawnEntities(spawnCountsSmallGroup[i], i);
			}
		} else if (LargeGroup == 1) {
			for (size_t i = 0; i < spawnCountsLargeGroup.size(); i++) {
				spawnEntities(spawnCountsLargeGroup[i], i);
			}
		}


		std::vector<std::vector<Entity*>> entityGroups = {
			Game::entityManager->GetEntitiesByLOT(16850), // wolfEntities
			Game::entityManager->GetEntitiesByLOT(16938), // scientistEntities
			Game::entityManager->GetEntitiesByLOT(32336), // beetleEntities
			Game::entityManager->GetEntitiesByLOT(16853), // bonezaiEntities
			Game::entityManager->GetEntitiesByLOT(16846), // overseerEntities
			Game::entityManager->GetEntitiesByLOT(14001), // handEntities
			Game::entityManager->GetEntitiesByLOT(16848), // marksmanEntities
			Game::entityManager->GetEntitiesByLOT(32335)  // blacksmithEntities
		};

		for (auto& entityGroup : entityGroups) {
			for (auto* entity : entityGroup) {
				auto* CombatAI = entity->GetComponent<BaseCombatAIComponent>();
				if (CombatAI != nullptr) {
					CombatAI->SetDisabled(true);
				}    
				entity->AddDieCallback([self, this]() {    
					Wave3Progress++;
					CheckWaveProgress(self);    
				});    
			}
		}
		
	} else if (timerName == "FinalSpinnersChain1") {	
		self->AddTimer("FinalSpinnersChain2", 1);	
		HandleSpinner(self, "overseer", "up");	
	} else if (timerName == "FinalSpinnersChain2") {	
		self->AddTimer("FinalSpinnersChain3", 1.1);	
		HandleSpinner(self, "wolf", "up");	
	} else if (timerName == "FinalSpinnersChain3") {	
		self->AddTimer("FinalSpinnersChain4", 1);	
		HandleSpinner(self, "beetle", "up");	
	} else if (timerName == "FinalSpinnersChain4") {	
		self->AddTimer("FinalSpinnersChain5", 1.1);	
		HandleSpinner(self, "marksman", "up");	
	} else if (timerName == "FinalSpinnersChain5") {	
		self->AddTimer("FinalSpinnersChain6", 1);	
		HandleSpinner(self, "blacksmith", "up");			
	} else if (timerName == "FinalSpinnersChain6") {	
		self->AddTimer("FinalSpinnersChain7", 1.1);		
		HandleSpinner(self, "scientist", "up");			
	} else if (timerName == "FinalSpinnersChain7") {	
		self->AddTimer("FinalSpinnersChain8", 1);	
		HandleSpinner(self, "hand", "up");			
	} else if (timerName == "FinalSpinnersChain8") {	
		HandleSpinner(self, "bonezai", "up");		
		
	} else if (timerName == "DoorActive") {
//		Communicate ready to spinner
		auto WavesSpinner = Game::entityManager->GetEntitiesInGroup("WavesElevatorSpinner");	
        for (auto* spinner : WavesSpinner) {		
			spinner->NotifyObject(self, "WavesOver");	
		}	
	} else if (timerName == "SpawnBouncer") {
		auto WavesBouncer = Game::zoneManager->GetSpawnersInGroup("WavesBuild02");	
        for (auto* bouncer : WavesBouncer) {		
			bouncer->Activate();
		}
	} else if (timerName == "FinalWaveLargeAudio") {			

		self->SetNetworkVar(u"wave3large", 1);
		self->AddTimer("TelePlayers", 3.3);			 	
		self->AddTimer("FinalWaveMusic", 19.5f);	
		
	} else if (timerName == "FinalWaveSmallAudio") {

		self->SetNetworkVar(u"wave3small", 1);		
		self->AddTimer("TelePlayers", 1.1);			
		self->SetNetworkVar(u"wave3music", 1);	
		
	} else if (timerName == "FinalWaveMusic") {
		self->SetNetworkVar(u"wave3music", 1);					
	} else if (timerName == "MusicOff") {
		
//		Move to client to handle		
		self->SetNetworkVar(u"stopmusic", 1);		
	} else if (timerName == "TelePlayers") {
	
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
		
//		Handle player spawns for wave 3

		NiPoint3 pos1{};
		pos1.SetX(-1223);  
		pos1.SetY(358);  
		pos1.SetZ(-166);  		
		
		NiPoint3 pos2{};
		pos2.SetX(-1233);  
		pos2.SetY(358);  
		pos2.SetZ(-166);  	

		NiPoint3 pos3{};
		pos3.SetX(-1239);  
		pos3.SetY(358);  
		pos3.SetZ(-169); 	

		NiPoint3 pos4{};
		pos4.SetX(-1219);  
		pos4.SetY(358);  
		pos4.SetZ(-169);  
		
		NiPoint3 TestPos{};
		TestPos.SetX(-1228.7377);  
		TestPos.SetY(358.1340);  
		TestPos.SetZ(-273.6017); 	
		

		if (engaged1 != nullptr && proximityMonitorComponent->IsInProximity("Area_Radius", engaged1->GetObjectID())
		&& WaveNum == 3) {	
			GameMessages::SendTeleport(engaged1->GetObjectID(), pos1, self->GetRotation(), engaged1->GetSystemAddress(), true);

			GameMessages::SendPlayerReachedRespawnCheckpoint(engaged1, TestPos, self->GetRotation());		
		} if (engaged2 != nullptr && proximityMonitorComponent->IsInProximity("Area_Radius", engaged2->GetObjectID())
		&& WaveNum == 3) {		
			GameMessages::SendTeleport(engaged2->GetObjectID(), pos2, self->GetRotation(), engaged2->GetSystemAddress(), true);

			GameMessages::SendPlayerReachedRespawnCheckpoint(engaged2, TestPos, self->GetRotation());		
		} if (engaged3 != nullptr && proximityMonitorComponent->IsInProximity("Area_Radius", engaged3->GetObjectID())
		&& WaveNum == 3) {		
			GameMessages::SendTeleport(engaged3->GetObjectID(), pos3, self->GetRotation(), engaged3->GetSystemAddress(), true);

			GameMessages::SendPlayerReachedRespawnCheckpoint(engaged3, TestPos, self->GetRotation());		
		} if (engaged4 != nullptr && proximityMonitorComponent->IsInProximity("Area_Radius", engaged4->GetObjectID())
		&& WaveNum == 3) {			
			GameMessages::SendTeleport(engaged4->GetObjectID(), pos4, self->GetRotation(), engaged4->GetSystemAddress(), true);

			GameMessages::SendPlayerReachedRespawnCheckpoint(engaged4, TestPos, self->GetRotation());
		}		
	}
}

