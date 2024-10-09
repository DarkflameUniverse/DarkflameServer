#include "FtBossManager.h"
#include "ProximityMonitorComponent.h"
#include "BuffComponent.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "RenderComponent.h"
#include "BaseCombatAIComponent.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"

Entity* FtBossManager::player1 = nullptr;
Entity* FtBossManager::player2 = nullptr;
Entity* FtBossManager::player3 = nullptr;
Entity* FtBossManager::player4 = nullptr;

Entity* FtBossManager::randomPlayer = nullptr;

void FtBossManager::OnStartup(Entity* self) {
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
	
	self->SetProximityRadius(160, "Start_Radius");	
	self->SetProximityRadius(4500, "Zone_Radius");	
	self->SetProximityRadius(130, "Combat_Radius");		
	
	LargeTeam = 1;
	
	self->SetVar<int>(u"DoorsOpen", 0);

}

void FtBossManager::OnCollisionPhantom(Entity* self, Entity* target) {
//	TODO Move logic from combat_Radius into collision phantom	
	
}	

void FtBossManager::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();		

	if (name == "Zone_Radius") {	
		if (entering->IsPlayer()) {
//		collect & store player entities	
			if (entering != player1 && entering != player2 && entering != player3 && entering != player4) {	
				
				if (!player1) {
					player1 = entering;
					self->SetVar<LWOOBJID>(u"playerid1", entering->GetObjectID());
					PlayerCount++;					
				} else if (!player2) {
					player2 = entering;	
					self->SetVar<LWOOBJID>(u"playerid2", entering->GetObjectID());	
					PlayerCount++;					
				} else if (!player3) {
					player3 = entering;
					self->SetVar<LWOOBJID>(u"playerid3", entering->GetObjectID());	
					PlayerCount++;						
				} else if (!player4) {
					player4 = entering;
					self->SetVar<LWOOBJID>(u"playerid4", entering->GetObjectID());
					PlayerCount++;					
				}
			}				
			
			
		}	
	}

	if (name == "Start_Radius") {

		
//		if (bStarted == 1 || self->GetVar<int>(u"DoorsOpen") != 6) {
		if (bStarted == 1) {
			return;
		}	
	
	
		
		if (entering->IsPlayer()) {			
		
//			Figure out team size
			LargeTeam = 1;
			if (self->GetVar<int>(u"playercount") == 1 || self->GetVar<int>(u"playercount") == 2) {
				LargeTeam = 0;
			}	

//			TODO Require more than 1 player @ boss if large team
			if (3 <= self->GetVar<int>(u"playercount")) {
				const auto FirstID = self->GetVar<LWOOBJID>(u"firstplayerid");
				auto* FirstEntity = Game::entityManager->GetEntity(FirstID);
				if (FirstEntity == nullptr || FirstEntity == entering) {
					self->SetVar<LWOOBJID>(u"firstplayerid", entering->GetObjectID());
					return;
				} else {
					bStarted = 1;	
				}
			} else {
				bStarted = 1;
			}	
			


//			Generate new seed for rng
			double randomPos = entering->GetPosition().x; 
			unsigned int seed = static_cast<unsigned int>(randomPos * 1000000); // Convert to int
			srand(seed);



			PlayCelebration(self, 26);

			self->AddTimer("SpawnStartingPortal", 3);

//			Teleport players 
			self->AddTimer("TelePlayers", 0.2f);

		}	
	}

}

void FtBossManager::PlayCelebration(Entity* self, int cutscene) {	
		
//	Turn off health bar before cutscene	27
	if (cutscene == 27) {	
		self->SetNetworkVar(u"TornadoDead", 1);	
	}
	
	
    Entity* playerEntities[] = {player1, player2, player3, player4};	
    for (Entity* player : playerEntities) {	
		if (player != nullptr) {
			auto* buffComponent = player->GetComponent<BuffComponent>();			
//			Clear poison first		
		
			if (buffComponent != nullptr) {
				buffComponent->RemoveBuff(60);
			}
			
			if (cutscene == 26) {
				GameMessages::SendStartCelebrationEffect(player, player->GetSystemAddress(), 26);							
			}	
			
			if (cutscene == 27) {			
				GameMessages::SendStartCelebrationEffect(player, player->GetSystemAddress(), 27);
	
	
//				Progress mission
				auto* missionComponent = player->GetComponent<MissionComponent>();	
				if (missionComponent != nullptr) {	
				
					missionComponent->Progress(eMissionTaskType::SCRIPT, self->GetLOT());
				}			
				
				
			}			
			
		}	
	}

	if (cutscene == 26) {				
		self->AddTimer("Celebration26Completed", 19.5f);					
	}	
			
	if (cutscene == 27) {			
		self->AddTimer("Celebration27Completed", 13.9f);	
	}	
	
}

void FtBossManager::TeleportPlayers(Entity* self) {
	
//	Player spawns

    NiPoint3 pos1{};
    pos1.SetX(-2235.25);  
    pos1.SetY(264.137);  
    pos1.SetZ(-475);  		
	
    NiPoint3 pos2{};
    pos2.SetX(-2220.75);  
    pos2.SetY(264.137);  
    pos2.SetZ(-475);  	

    NiPoint3 pos3{};
    pos3.SetX(-2257.5);  
    pos3.SetY(264.137);  
    pos3.SetZ(-479); 	

    NiPoint3 pos4{};
    pos4.SetX(-2200.5);  
    pos4.SetY(264.137);  
    pos4.SetZ(-479);  
	
    NiPoint3 TestPos{};
    TestPos.SetX(-2228.2815);  
    TestPos.SetY(280.3420);  
    TestPos.SetZ(-374.2848); 		
	
//	Teleport players to proper positions
	if (player1 != nullptr) {	
		GameMessages::SendTeleport(player1->GetObjectID(), pos1, self->GetRotation(), player1->GetSystemAddress(), 
		true);	
		
		GameMessages::SendPlayerReachedRespawnCheckpoint(player1, TestPos, self->GetRotation());		
	} if (player2 != nullptr) {		
		GameMessages::SendTeleport(player2->GetObjectID(), pos2, self->GetRotation(), player2->GetSystemAddress(), 
		true);	
		
		GameMessages::SendPlayerReachedRespawnCheckpoint(player2, TestPos, self->GetRotation());				
	} if (player3 != nullptr) {		
		GameMessages::SendTeleport(player3->GetObjectID(), pos3, self->GetRotation(), player3->GetSystemAddress(), 
		true);	
		
		GameMessages::SendPlayerReachedRespawnCheckpoint(player3, TestPos, self->GetRotation());				
	} if (player4 != nullptr) {			
		GameMessages::SendTeleport(player4->GetObjectID(), pos4, self->GetRotation(), player4->GetSystemAddress(),
		true);	

		GameMessages::SendPlayerReachedRespawnCheckpoint(player4, TestPos, self->GetRotation());				
	}
//	end		

	self->SetNetworkVar(u"WipeMusic", 2);	
}

void FtBossManager::StartElement(Entity* self, std::string elementString) {
	const auto portalSpawner = Game::zoneManager->GetSpawnersByName("BossPortalSpawn_" + elementString);
	const auto endpostSpawner = Game::zoneManager->GetSpawnersByName("BossEndposts_" + elementString);
	const auto activatorSpawner = Game::zoneManager->GetSpawnersByName("BossActivators_" + elementString);


//	rng jumble
	static std::vector<int> availableNodes = {0, 1, 2, 3, 4, 5};
	static bool initialized = false;
	if (!initialized) {
		for (int i = availableNodes.size() - 1; i > 0; --i) {
			int j = rand() % (i + 1);
			std::swap(availableNodes[i], availableNodes[j]);
		}
		initialized = true;
	}

	if (availableNodes.empty()) {
		availableNodes = {0, 1, 2, 3, 4, 5};
		for (int i = availableNodes.size() - 1; i > 0; --i) {
			int j = rand() % (i + 1);
			std::swap(availableNodes[i], availableNodes[j]);
		}
	}

	nextNode = availableNodes.back();
	availableNodes.pop_back();
	self->SetVar<int>(u"randNum", nextNode);
	//END	
	
	CurrentElement = elementString;
	
	for (auto* portal : portalSpawner) {	
		TeleportPortal(self, portal, nextNode);
	}	

	
	PortalAlreadyHitOnce = false;
	pauseMortar = false;
	
//	Startup rails	

	for (auto* endpost : endpostSpawner) {	
		endpost->Spawn({
		endpost->m_Info.nodes.at(nextNode)
    }, true);
	}
	
	for (auto* activator : activatorSpawner) {	
		activator->Spawn({
		activator->m_Info.nodes.at(nextNode)
    }, true);
	}	


}	

void FtBossManager::TeleportPortal(Entity* self, Spawner* spawner, int nodeNum) {

//	Destroy portal	
	spawner->Deactivate();
	spawner->DestroyAllEntities();
	
	spawner->Spawn({
		spawner->m_Info.nodes.at(nodeNum)
    }, true);
	

	PortalHit = false;
}

void FtBossManager::PlayerHitPortal(Entity* self, std::string element) {
	
	
	
	if (PortalHit || element != CurrentElement){
		return;
	}
	PortalHit = true;	
	pauseMortar = true;
	
	const auto NodeStr = std::to_wstring(nextNode + 1);	
	
//		Play correct cine
		Entity* playerEntities[] = {player1, player2, player3, player4};
		for (Entity* player : playerEntities) {	
			if (player != nullptr) {
			
				std::u16string cinematicName = u"Boss_RailPedestal_0";
				cinematicName += std::u16string(NodeStr.begin(), NodeStr.end());
				cinematicName += u"_hit";
            
				GameMessages::SendPlayCinematic(player->GetObjectID(), cinematicName, player->GetSystemAddress());
				
			}	
		}	
	

//	update health bar	
	self->AddTimer("UpdateHealthBar", 0.5f);			


//	start new wave	
	self->AddTimer("StartNextWave", 4);		
	
	
}

void FtBossManager::StartNewWave(Entity* self) {
	
	
	if (waveNum != 4) {
		
//		Play cines	
		Entity* playerEntities[] = {player1, player2, player3, player4};
		for (Entity* player : playerEntities) {	
			if (player != nullptr) {
			
				GameMessages::SendPlayCinematic(player->GetObjectID(), u"BossSpawnWaveCam", 
				player->GetSystemAddress());
				
			}	
		}
		
//		Send play tornado fx
		const auto TornadoEntity = Game::entityManager->GetEntitiesByLOT(16807);	
			
		for (auto* tornado : TornadoEntity) {	
			tornado->NotifyObject(self, "PlaySummonAnim");
		}
		
//		Move spinners here
		self->AddTimer("BossSpinnersUp", 0.5f);	
		self->AddTimer("BossSpinnersDown", 6.3f);

//		Spawn enemies

		self->AddTimer("SpawnEnemies", 3.7f);	
	} else {
	
////////////////////	
//	START PHASE 2 /
//////////////////

		PlayCelebration(self, 27);

//		Prevent cam glitch with celebration
		Entity* playerEntities[] = {player1, player2, player3, player4};
		for (Entity* player : playerEntities) {	
			if (player != nullptr) {
				for (int i = 1; i <= 6; ++i) {
					std::wstring num = std::to_wstring(i);
					std::u16string cinematicName = u"Boss_RailPedestal_0";
					cinematicName += std::u16string(num.begin(), num.end());
					cinematicName += u"_hit";

					GameMessages::SendEndCinematic(player->GetObjectID(), cinematicName, player->GetSystemAddress());
				}
			}
		}



//		Teleport players 
		self->AddTimer("TelePlayers", 0.2f);

		
		self->AddTimer("SwapGarmadon", 0.6f);		



	}
}

void FtBossManager::SpinnerEnemies(Entity* self) {

//	Reuse wave 2 spawner for wave 4
    int spinnerWaveNum = (waveNum == 4) ? 2 : waveNum;

//	Get correct spinner spawn nw
    auto spinner1 = Game::zoneManager->GetSpawnersByName("BossSpawnerSpinner_1_" + std::to_string(spinnerWaveNum));
    auto spinner2 = Game::zoneManager->GetSpawnersByName("BossSpawnerSpinner_2_" + std::to_string(spinnerWaveNum));

    if (spinner1.empty() || spinner2.empty()) return;

    auto* spawner1 = spinner1.front();
    auto* spawner2 = spinner2.front();

    // wave data
    std::vector<std::vector<std::tuple<std::string, int, int, int>>> waves = {
        // Wave 1
        {
            {"hand", 2, 2, 14002},
            {"blacksmith", 3, 1, 32335},
            {"beetle", 2, 0, 13998}
        },
        // Wave 2
        {
            {"beetle", 1, 0, 13998},
            {"blacksmith", 2, 2, 32335},
            {"vulture", 3, 2, 14000},
            {"overseer", 2, 1, 16847}
        },		
        // Wave 3		
        {
            {"blacksmith", 2, 1, 32335},
            {"hand", 4, 2, 14002},
            {"vulture", 2, 2, 14000},
            {"marksman", 2, 0, 16849}
        },
        // Wave 4
        {
            {"marksman", 2, 2, 16849},
            {"beetle", 1, 1, 13998},
            {"blacksmith", 2, 2, 32335},
          {"wolf", 2, 0, 16191}
        }		
    };

    // Get the current wave
    int waveIndex = waveNum - 1; 
    if (waveIndex < 0 || waveIndex >= waves.size()) return;

   // waypoints
    int waypointIndex1 = 0;
    int waypointIndex2 = 0;

    // Distribute the enemies to both spinners
    for (const auto& enemy : waves[waveIndex]) {
        std::string enemyType = std::get<0>(enemy);
        int largeGroupCount = std::get<1>(enemy);
        int smallGroupCount = std::get<2>(enemy);
        int spawnLot = std::get<3>(enemy);

        int totalToSpawn = (LargeTeam == 1) ? largeGroupCount : smallGroupCount;
        int halfToSpawn = totalToSpawn / 2;

        // spinner1
        spawner1->SetSpawnLot(spawnLot);
        for (int i = 0; i < halfToSpawn; ++i) {
            spawner1->Spawn({ spawner1->m_Info.nodes.at(waypointIndex1) }, true);
            waypointIndex1 = (waypointIndex1 + 1) % spawner1->m_Info.nodes.size();
        }

        // spinner2
        spawner2->SetSpawnLot(spawnLot);
        for (int i = 0; i < (totalToSpawn - halfToSpawn); ++i) {
            spawner2->Spawn({ spawner2->m_Info.nodes.at(waypointIndex2) }, true);
            waypointIndex2 = (waypointIndex2 + 1) % spawner2->m_Info.nodes.size();
        }
    }

	if (waveNum != 4) {
		RunDieCallbacks(self);
	
//	Stun/unstun	
		StunEnemies(self, true);		
		self->AddTimer("UnstunEnemies", 5.2f);
	}
}

void FtBossManager::RunDieCallbacks(Entity* self) {
	
	auto beetleEntities = Game::entityManager->GetEntitiesByLOT(13998);
	auto blacksmithEntities = Game::entityManager->GetEntitiesByLOT(32335);
	auto vultureEntities = Game::entityManager->GetEntitiesByLOT(14000);
	auto overseerEntities = Game::entityManager->GetEntitiesByLOT(16847);
	auto handEntities = Game::entityManager->GetEntitiesByLOT(14002);
	auto marksmanEntities = Game::entityManager->GetEntitiesByLOT(16849);			

	for (auto* beetle : beetleEntities) {
		beetle->AddDieCallback([self, this]() {
			CheckProgress(self);
		});
	}


	for (auto* blacksmith : blacksmithEntities) {
		blacksmith->AddDieCallback([self, this]() {
			CheckProgress(self);
		});
	}

	for (auto* vulture : vultureEntities) {
		vulture->AddDieCallback([self, this]() {
			CheckProgress(self);
		});
	}

	for (auto* overseer : overseerEntities) {
		overseer->AddDieCallback([self, this]() {
			CheckProgress(self);
		});
	}

	for (auto* hand : handEntities) {
		hand->AddDieCallback([self, this]() {
			CheckProgress(self);
		});
	}

	for (auto* marksman : marksmanEntities) {
		marksman->AddDieCallback([self, this]() {
			CheckProgress(self);
		});
	}		
}

void FtBossManager::StunEnemies(Entity* self, bool value) {
	
	struct {
		uint32_t LOT;
		std::vector<Entity*> entities;
	} entityGroups[] = {
		{ 13998, Game::entityManager->GetEntitiesByLOT(13998) },   // beetleEntities
		{ 32335, Game::entityManager->GetEntitiesByLOT(32335) },   // blacksmithEntities
		{ 14000, Game::entityManager->GetEntitiesByLOT(14000) },   // vultureEntities
		{ 16847, Game::entityManager->GetEntitiesByLOT(16847) },   // overseerEntities
		{ 14002, Game::entityManager->GetEntitiesByLOT(14002) },   // handEntities
		{ 16849, Game::entityManager->GetEntitiesByLOT(16849) },   // marksmanEntities
		{ 16854, Game::entityManager->GetEntitiesByLOT(16854) },   // frakjawEntities
		{ 16846, Game::entityManager->GetEntitiesByLOT(16846) },   // overseerEntitiesFrakjaw	
	};

	for (auto& group : entityGroups) {
		for (auto* entity : group.entities) {
			auto* CombatAI = entity->GetComponent<BaseCombatAIComponent>();
			if (CombatAI != nullptr) {
				CombatAI->SetDisabled(value);
			}
		}
	}	
	
}	

void FtBossManager::CheckProgress(Entity* self) {
//	Progress current wave if called	

	//	Wave 1 //
	
	if (waveNum == 1) {
		wave1Progress++;
		if (LargeTeam == 0 && wave1Progress == 3) {
			self->AddTimer("WaveOver", 3.9f);
		
		} else if (LargeTeam == 1 && wave1Progress == 7) {
			self->AddTimer("WaveOver", 3.9f);	
			
		}	
		
		
	// Wave 2 //
		
	} else if (waveNum == 2) {
		wave2Progress++;
		if (LargeTeam == 0 && wave2Progress == 5) {
			self->AddTimer("WaveOver", 3.9f);
		
		} else if (LargeTeam == 1 && wave2Progress == 8) {
			self->AddTimer("WaveOver", 3.9f);
			
		}			
		
		
	// Wave 3 //	
		
	} else if (waveNum == 3) {	
		wave3Progress++;
		if (LargeTeam == 0 && wave3Progress == 5) {
			self->AddTimer("CallFrakjaw", 3.1f);
		
		} else if (LargeTeam == 1 && wave3Progress == 10) {
			self->AddTimer("CallFrakjaw", 3.1f);
				
		} else if (LargeTeam == 0 && wave3Progress == 6) {
			self->AddTimer("WaveOver", 3.9f);
			
		} else if (LargeTeam == 1 && wave3Progress == 13) {
			self->AddTimer("WaveOver", 3.9f);
			
		}					
		
	} 
	
	
	
}

void FtBossManager::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, 
int32_t param1, int32_t param2) {
	
	if (name == "ActivatorUsed" && param1 == self->GetVar<int32_t>(u"PortalNode")) {



		self->CancelTimer("TeleportPortal");
	}
	else if (name == "SendEndpostOn") {
		for (int i = 1; i <= 6; ++i) {
			auto EndpostEntity = Game::entityManager->GetEntitiesInGroup("BossEndpost_" + std::to_string(i) + "_" + CurrentElement);
			for (auto* endpost : EndpostEntity) {
				endpost->SetNetworkVar<bool>(u"NetworkNotActive", false);    
				endpost->SetVar<bool>(u"NotActive", false);    
			}
		}
		
	}		
	else if (name == "EarthRailArrived") {
		for (int i = 1; i <= 6; ++i) {
			auto EndpostEntity = Game::entityManager->GetEntitiesInGroup("BossEndpost_" + std::to_string(i) + "_earth");
			for (auto* endpost : EndpostEntity) {
				endpost->SetNetworkVar<bool>(u"NetworkNotActive", true);    
				endpost->SetVar<bool>(u"NotActive", true);    
			}
		}		
		
		PlayerHitPortal(self, "earth");
	}	
	else if (name == "LightningRailArrived") {
		for (int i = 1; i <= 6; ++i) {
			auto EndpostEntity = Game::entityManager->GetEntitiesInGroup("BossEndpost_" + std::to_string(i) + "_lightning");
			for (auto* endpost : EndpostEntity) {
				endpost->SetNetworkVar<bool>(u"NetworkNotActive", true);    
				endpost->SetVar<bool>(u"NotActive", true);    
			}
		}		
		
		PlayerHitPortal(self, "lightning");
	}
	else if (name == "IceRailArrived") {
		for (int i = 1; i <= 6; ++i) {
			auto EndpostEntity = Game::entityManager->GetEntitiesInGroup("BossEndpost_" + std::to_string(i) + "_ice");
			for (auto* endpost : EndpostEntity) {
				endpost->SetNetworkVar<bool>(u"NetworkNotActive", true);    
				endpost->SetVar<bool>(u"NotActive", true);    
			}
		}		
		
		PlayerHitPortal(self, "ice");
	}
	else if (name == "FireRailArrived") {
		for (int i = 1; i <= 6; ++i) {
			auto EndpostEntity = Game::entityManager->GetEntitiesInGroup("BossEndpost_" + std::to_string(i) + "_fire");
			for (auto* endpost : EndpostEntity) {
				endpost->SetNetworkVar<bool>(u"NetworkNotActive", true);    
				endpost->SetVar<bool>(u"NotActive", true);    
			}
		}			
		
		PlayerHitPortal(self, "fire");
	}	
	else if (name == "SmashPortal") {
		self->AddTimer("SmashPortal", 0.5f);	
	}	
	else if (name == "RequestPlayer") {
		SpawnMortar(self, "Mortar");
		sender->NotifyObject(self, "PlayerSent");
	}		
}	



void FtBossManager::OnPlayerExit(Entity* self, Entity* player) {
	PlayerCount--;
	
	self->SetNetworkVar<LWOOBJID>(u"PlayerLeft", player->GetObjectID());				
}

void FtBossManager::OnTimerDone(Entity* self, std::string timerName) {
	Entity* playerEntities[] = {player1, player2, player3, player4};	
	
	
	if (timerName == "Celebration26Completed") {
		for (Entity* player : playerEntities) {	
			if (player != nullptr) {
			
				GameMessages::SendPlayCinematic(player->GetObjectID(), u"BossStartCam", 
				player->GetSystemAddress());	
			}	
		}
		

		
		self->AddTimer("KillStartingPortal", 9.5f);
		self->AddTimer("UnstunTornado", 10.2f);
		
		
//		Send start music		
		self->SetNetworkVar(u"MusicStart", 1);		
		
	} else if (timerName == "Celebration27Completed") {		
	
//		Switch music	
		self->SetNetworkVar(u"BossMusicStart", 1);
	
		for (Entity* player : playerEntities) {	
			if (player != nullptr) {
			
				GameMessages::SendPlayCinematic(player->GetObjectID(), u"BossLGCam", 
				player->GetSystemAddress());			
			}	
		}	
					
			
		self->AddTimer("UnstunGarmadon", 2.1f);				
		self->AddTimer("SpawnEnemies", 6.9f);		
			
		
	} else if (timerName == "TelePlayers") {	

		TeleportPlayers(self);
		
	} else if (timerName == "SpawnStartingPortal") {	
	
		self->SetNetworkVar(u"WipeMusic", 1);		
	
		const auto PortalSpawner = Game::zoneManager->GetSpawnersByName("BossCenterPortal");
		const auto TornadoSpawner = Game::zoneManager->GetSpawnersByName("BossLGTornado");
		
		for (auto* portal : PortalSpawner) {	
			portal->Activate();
		}
		for (auto* tornado : TornadoSpawner) {	
			tornado->Activate();
		}
		

		
	} else if (timerName == "KillStartingPortal") {
		const auto PortalSpawner = Game::zoneManager->GetSpawnersByName("BossCenterPortal");
		
		for (auto* portal : PortalSpawner) {	
			portal->Deactivate();
			portal->DestroyAllEntities();
		}	
		self->AddTimer("StartEarth", 2);	
		
		self->SetNetworkVar(u"TurnOnHealthBar", 1);				
						

	} else if (timerName == "UnstunTornado") {		
//		Send FindAPath to tornado

		const auto TornadoEntity = Game::entityManager->GetEntitiesByLOT(16807);	
		
		for (auto* tornado : TornadoEntity) {	
			tornado->NotifyObject(self, "FindAPath");
		}
		
	} else if (timerName == "StartEarth") {

		StartElement(self, "earth");
		
	} else if (timerName == "UpdateHealthBar") {		
//		update health bar	
		self->SetNetworkVar(u"UpdateHealthBar", waveNum);	
		
		if (waveNum == 4) {
//			Tell client switch music
			self->SetNetworkVar(u"MusicBattleOver", 1);			
		}
	} else if (timerName == "SmashPortal") {		
		const auto portalSpawner = Game::zoneManager->GetSpawnersByName("BossPortalSpawn_" + CurrentElement);
	
		for (auto* portal : portalSpawner) {				
			portal->Deactivate();
			portal->DestroyAllEntities();
		}		
	} else if (timerName == "StartNextWave") {		
//		spawn the next wave
	
		StartNewWave(self);	
	
	} else if (timerName == "BossSpinnersUp") {	
		auto BossSpinners = Game::entityManager->GetEntitiesInGroup("BossSpawnerSpinners");
		
		for (auto* spinner : BossSpinners) {	
			GameMessages::SendPlatformResync(spinner, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
			RenderComponent::PlayAnimation(spinner, u"up");
			GameMessages::SendPlayNDAudioEmitter(spinner, spinner->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");
			GameMessages::SendPlayFXEffect(spinner->GetObjectID(), 10102, u"create", "create");
		}	
	
	} else if (timerName == "BossSpinnersDown") {		
	auto BossSpinners = Game::entityManager->GetEntitiesInGroup("BossSpawnerSpinners");
	
		for (auto* spinner : BossSpinners) {
			GameMessages::SendPlatformResync(spinner, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1, 1, eMovementPlatformState::Moving);
			RenderComponent::PlayAnimation(spinner, u"down");
			GameMessages::SendPlayNDAudioEmitter(spinner, spinner->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");
		}	
	} else if (timerName == "SpawnEnemies") {	
		pauseMortar = false;
	
		SpinnerEnemies(self);
	} else if (timerName == "WaveOver") {
		
		pauseMortar = true;
		
//		Play cines for portal spawn
		Entity* playerEntities[] = {player1, player2, player3, player4};
		std::u16string uCurrentElement(CurrentElement.begin(), CurrentElement.end());	
		
		for (Entity* player : playerEntities) {	
			if (player != nullptr) {
				
//				Clear poison				
				auto* buffComponent = player->GetComponent<BuffComponent>();
				if (buffComponent != nullptr) {
					buffComponent->RemoveBuff(60);
				}					
				
				GameMessages::SendPlayCinematic(player->GetObjectID(), u"BossPortalCam_" + uCurrentElement, 
				player->GetSystemAddress());	
			}	
		}
		self->AddTimer("StartNextElement", 1.1f);			

	} else if (timerName == "StartNextElement") {	
	
		if (waveNum == 1) {
			StartElement(self, "lightning");
			waveNum++;
		} else if (waveNum == 2) {	
			StartElement(self, "ice");
			waveNum++;
		} else if (waveNum == 3) {	
			StartElement(self, "fire");
			waveNum++;
		}

	} else if (timerName == "CallFrakjaw") {
//		ring ring some dude with a tophat needs you back
//		Ah shit, here we go again	

		Entity* playerEntities[] = {player1, player2, player3, player4};
		for (Entity* player : playerEntities) {	
			if (player != nullptr) {
				
//				Clear poison					
				auto* buffComponent = player->GetComponent<BuffComponent>();
				if (buffComponent != nullptr) {
					buffComponent->RemoveBuff(60);
				}					
				
				GameMessages::SendPlayCinematic(player->GetObjectID(), u"BossWaveOverCam", 
				player->GetSystemAddress());	
			}	
		}

		self->AddTimer("SpawnFrakjaw", 3.1f);			
		
	} else if (timerName == "SpawnFrakjaw") {
			
		auto FrakjawSpawner = Game::zoneManager->GetSpawnersByName("FrakjawSpawner01");	
		auto overseerSpawner1 = Game::zoneManager->GetSpawnersByName("BossSpawnerSpinner_1_3");
		auto overseerSpawner2 = Game::zoneManager->GetSpawnersByName("BossSpawnerSpinner_2_3");		
		for (auto* spawner : FrakjawSpawner) {	
			spawner->Spawn();
		}		
		
		auto frakjawEntities = Game::entityManager->GetEntitiesByLOT(16854);			
		for (auto* frakjaw : frakjawEntities) {
			frakjaw->AddDieCallback([self, this]() {
				CheckProgress(self);
			});
		}
		
//		+ A couple minions for large team			

		if (LargeTeam == 1) {
			
			for (auto* spawner : overseerSpawner1) {	
				spawner->SetSpawnLot(16846);
				spawner->Spawn({
					spawner->m_Info.nodes.at(3)
					}, true);
			}	
			for (auto* spawner : overseerSpawner2) {	
				spawner->SetSpawnLot(16846);
				spawner->Spawn({
					spawner->m_Info.nodes.at(0)
					}, true);
			}		
			
			auto overseerEntities = Game::entityManager->GetEntitiesByLOT(16846);					
			for (auto* overseer : overseerEntities) {
				overseer->AddDieCallback([self, this]() {
					CheckProgress(self);
				});
			}			
			
		}
		
//		Stun/unstun	
		StunEnemies(self, true);		
		self->AddTimer("UnstunEnemies", 4.5f);	
		
	} else if (timerName == "UnstunEnemies") {		
	
		StunEnemies(self, false);
		
		
	} else if (timerName == "SwapGarmadon") {	
		auto garmadonSpawner = Game::zoneManager->GetSpawnersByName("BossLordGarmadon");			
		const auto TornadoEntity = Game::entityManager->GetEntitiesByLOT(16807);		

		
		for (auto* tornado : TornadoEntity) {
		
			tornado->Smash(tornado->GetObjectID(), eKillType::SILENT);
		}	
		
		
		for (auto* spawner : garmadonSpawner) {
			spawner->Spawn();
		}	
		
		auto garmadonEntity = Game::entityManager->GetEntitiesByLOT(16810);			
		for (auto* garmadon : garmadonEntity) {
			garmadon->AddDieCallback([self, this]() {
				self->AddTimer("GarmadonDefeated", 1);	
				
	
			});
			
			auto* CombatAI = garmadon->GetComponent<BaseCombatAIComponent>();
			if (CombatAI != nullptr) {
				CombatAI->SetDisabled(true);
			}				
		}	


	} else if (timerName == "UnstunGarmadon") {
		auto garmadonEntity = Game::entityManager->GetEntitiesByLOT(16810);			
		for (auto* garmadon : garmadonEntity) {	
//			garmadon's health bar		
			garmadon->SetNetworkVar(u"TurnOnHealthBar", 1);
			
			auto* CombatAI = garmadon->GetComponent<BaseCombatAIComponent>();
			if (CombatAI != nullptr) {
				CombatAI->SetDisabled(false);
			}	
		}	
		
		self->AddTimer("BossSpinnersUp", 1.9f);	
		self->AddTimer("BossSpinnersDown", 7.8f);			

	} else if (timerName == "GarmadonDefeated") {	

//		Tell client switch music
		self->SetNetworkVar(u"MusicBossOver", 1);	


//		Ensure all HealthBars are off			
		self->SetNetworkVar(u"TornadoDead", 2);	
		
//		Smash remaining enemies
		for (auto i = 1; i < 3; i++) {
			auto spawners = Game::zoneManager->GetSpawnersByName("BossSpawnerSpinner_" + 
			std::to_string(i) + "_2");
			if (!spawners.empty()) {
				auto* spawner = spawners.front();
				spawner->Deactivate();
				spawner->Reset();
			}
		}	
		
//		Play bouncer cine
		Entity* playerEntities[] = {player1, player2, player3, player4};
		for (Entity* player : playerEntities) {	
			if (player != nullptr) {          
//				Clear poison				
				auto* buffComponent = player->GetComponent<BuffComponent>();
				if (buffComponent != nullptr) {
					buffComponent->RemoveBuff(60);
				}							
				GameMessages::SendPlayCinematic(player->GetObjectID(), u"BossOverCam", player->GetSystemAddress());	
				
//				Force music guid here

				GameMessages::SendPlayNDAudioEmitter(player, player->GetSystemAddress(), 
				"{13084a0a-99a0-48db-9a3e-ce1edf318b7d}");
//				end				
				
			}	
		}	

		self->AddTimer("SpawnBouncer", 4);	
		
	} else if (timerName == "SpawnBouncer") {
	
		const auto bouncerSpawner = Game::zoneManager->GetSpawnersByName("BossEndBouncer");
		
		for (auto* bouncer : bouncerSpawner) {	
			bouncer->Activate();
		}	


//		Remove invis physics object blocking chest
		const auto ChestEntity = Game::entityManager->GetEntitiesByLOT(32115);				
		for (auto* chest : ChestEntity) {	
			chest->Smash(chest->GetObjectID(), eKillType::SILENT);
		}		
	}
}

void FtBossManager::SpawnMortar(Entity* self, const std::string& loc) {

    if (pauseMortar == true) {
        return;
    }


	randomPlayerNum = 0;


    std::vector<int> playersInGame;


    if (player1 != nullptr) {
        playersInGame.push_back(1);
    }
    if (player2 != nullptr) {
        playersInGame.push_back(2);
    }
    if (player3 != nullptr) {
        playersInGame.push_back(3);
    }
    if (player4 != nullptr) {
        playersInGame.push_back(4);
    }

    if (!playersInGame.empty()) {

        

        int randomIndex = rand() % playersInGame.size();
        randomPlayerNum = playersInGame[randomIndex];


    }

	if (randomPlayerNum = 0) {
		randomPlayer = nullptr;		
	} else if (randomPlayerNum = 1) {
		randomPlayer = player1;		
	} else if (randomPlayerNum = 2) {
		randomPlayer = player2;		
	} else if (randomPlayerNum = 3) {
		randomPlayer = player3;
	} else if (randomPlayerNum = 4) {
		randomPlayer = player4;
	}	

    if (randomPlayer == nullptr) {
        return;
    }
	
	
	auto pos = self->GetPosition();
	auto rot = self->GetRotation();
	pos.y += self->GetVarAs<float>(u"vert_offset");

	auto newRot = rot;
	auto offset = self->GetVarAs<float>(u"hort_offset");

	std::vector<LDFBaseData*> config = { new LDFData<std::string>(u"Leg", loc) };

	EntityInfo info{};
	info.lot = 16891;
	info.spawnerID = self->GetObjectID();
	info.settings = config;
	info.rot = newRot;

	if (loc == "Mortar") {
		const auto dir = rot.GetRightVector();
		pos.x = randomPlayer->GetPosition().x;
		pos.y = randomPlayer->GetPosition().y;
		pos.z = randomPlayer->GetPosition().z;
		info.pos = pos;
	}

	info.rot = NiQuaternion::LookAt(info.pos, self->GetPosition());

	auto* entity = Game::entityManager->CreateEntity(info);

	Game::entityManager->ConstructEntity(entity);

}
