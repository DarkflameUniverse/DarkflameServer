#include "NjMonastryBossInstance.h"
#include "RebuildComponent.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "GameMessages.h"
#include "BaseCombatAIComponent.h"
#include "BuffComponent.h"
#include "SkillComponent.h"
#include "TeamManager.h"
#include <algorithm>

// // // // // // //
// Event handling //
// // // // // // //

void NjMonastryBossInstance::OnStartup(Entity* self) {
	auto spawnerNames = std::vector<std::string>{ LedgeFrakjawSpawner, LowerFrakjawSpawner, BaseEnemiesSpawner + std::to_string(1),
									  BaseEnemiesSpawner + std::to_string(2), BaseEnemiesSpawner + std::to_string(3),
									  BaseEnemiesSpawner + std::to_string(4), CounterweightSpawner };

	// Add a notification request for all the spawned entities, corresponds to notifySpawnedObjectLoaded
	for (const auto& spawnerName : spawnerNames) {
		for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(spawnerName)) {
			spawner->AddEntitySpawnedCallback([self, this](Entity* entity) {
				const auto lot = entity->GetLOT();
				switch (lot) {
				case LedgedFrakjawLOT:
					NjMonastryBossInstance::HandleLedgedFrakjawSpawned(self, entity);
					return;
				case CounterWeightLOT:
					NjMonastryBossInstance::HandleCounterWeightSpawned(self, entity);
					return;
				case LowerFrakjawLOT:
					NjMonastryBossInstance::HandleLowerFrakjawSpawned(self, entity);
					return;
				default:
					NjMonastryBossInstance::HandleWaveEnemySpawned(self, entity);
					return;
				}
				});
		}
	}
}

void NjMonastryBossInstance::OnPlayerLoaded(Entity* self, Entity* player) {
	ActivityTimerStop(self, WaitingForPlayersTimer);

	// Join the player in the activity
	UpdatePlayer(self, player->GetObjectID());

	// Buff the player
	auto* destroyableComponent = player->GetComponent<DestroyableComponent>();
	if (destroyableComponent != nullptr) {
		destroyableComponent->SetHealth((int32_t)destroyableComponent->GetMaxHealth());
		destroyableComponent->SetArmor((int32_t)destroyableComponent->GetMaxArmor());
		destroyableComponent->SetImagination((int32_t)destroyableComponent->GetMaxImagination());
	}

	// Add player ID to instance
	auto totalPlayersLoaded = self->GetVar<std::vector<LWOOBJID>>(TotalPlayersLoadedVariable);
	totalPlayersLoaded.push_back(player->GetObjectID());

	// Properly position the player
	self->SetVar<std::vector<LWOOBJID>>(TotalPlayersLoadedVariable, totalPlayersLoaded);
	// This was always spawning all players at position one before and other values cause players to be invisible.
	TeleportPlayer(player, 1);

	// Large teams face a tougher challenge
	if (totalPlayersLoaded.size() >= 3)
		self->SetVar<bool>(LargeTeamVariable, true);

	// Start the game if all players in the team have loaded
	auto* team = TeamManager::Instance()->GetTeam(player->GetObjectID());
	if (team == nullptr || totalPlayersLoaded.size() == team->members.size()) {
		StartFight(self);
		return;
	}

	self->AddCallbackTimer(0.0f, [self, player]() {
		if (player != nullptr) {
			// If we don't have enough players yet, wait for the others to load and notify the client to play a cool cinematic
			GameMessages::SendNotifyClientObject(self->GetObjectID(), u"PlayerLoaded", 0, 0,
				player->GetObjectID(), "", player->GetSystemAddress());
		}
		});

	ActivityTimerStart(self, WaitingForPlayersTimer, 45.0f, 45.0f);
}

void NjMonastryBossInstance::OnPlayerExit(Entity* self, Entity* player) {
	UpdatePlayer(self, player->GetObjectID(), true);
	// Fetch the total players loaded from the vars
	auto totalPlayersLoaded = self->GetVar<std::vector<LWOOBJID> >(TotalPlayersLoadedVariable);

	// Find the player to remove
	auto playerToRemove = std::find(totalPlayersLoaded.begin(), totalPlayersLoaded.end(), player->GetObjectID());

	// If we found the player remove them from out list of players
	if (playerToRemove != totalPlayersLoaded.end()) {
		totalPlayersLoaded.erase(playerToRemove);
	} else {
		Game::logger->Log("NjMonastryBossInstance", "Failed to remove player at exit.");
	}

	// Set the players loaded var back
	self->SetVar<std::vector<LWOOBJID>>(TotalPlayersLoadedVariable, totalPlayersLoaded);

	// Since this is an exit method, check if enough players have left.  If enough have left
	// resize the instance to account for such.
	if (totalPlayersLoaded.size() <= 2) self->SetVar<bool>(LargeTeamVariable, false);

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"PlayerLeft", 0, 0, player->GetObjectID(), "", UNASSIGNED_SYSTEM_ADDRESS);
}

void NjMonastryBossInstance::OnActivityTimerDone(Entity* self, const std::string& name) {
	auto split = GeneralUtils::SplitString(name, TimerSplitChar);
	auto timerName = split[0];
	auto objectID = split.size() > 1 ? (LWOOBJID)std::stoull(split[1]) : LWOOBJID_EMPTY;

	if (timerName == WaitingForPlayersTimer) {
		StartFight(self);
	} else if (timerName == SpawnNextWaveTimer) {
		auto* frakjaw = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(LedgeFrakjawVariable));
		if (frakjaw != nullptr) {
			SummonWave(self, frakjaw);
		}
	} else if (timerName == SpawnWaveTimer) {
		auto wave = self->GetVar<uint32_t>(WaveNumberVariable);
		self->SetVar<uint32_t>(WaveNumberVariable, wave + 1);
		self->SetVar<uint32_t>(TotalAliveInWaveVariable, 0);

		if (wave < m_Waves.size()) {
			auto waves = m_Waves.at(wave);
			auto counter = 0;

			for (const auto& waveEnemy : waves) {
				const auto numberToSpawn = self->GetVar<bool>(LargeTeamVariable)
					? waveEnemy.largeNumber : waveEnemy.smallNumber;

				auto spawnIndex = counter % 4 + 1;
				SpawnOnNetwork(self, waveEnemy.lot, numberToSpawn, BaseEnemiesSpawner + std::to_string(spawnIndex));
				counter++;
			}
		}
	} else if (timerName + TimerSplitChar == UnstunTimer) {
		auto* entity = EntityManager::Instance()->GetEntity(objectID);
		if (entity != nullptr) {
			auto* combatAI = entity->GetComponent<BaseCombatAIComponent>();
			if (combatAI != nullptr) {
				combatAI->SetDisabled(false);
			}
		}
	} else if (timerName == SpawnCounterWeightTimer) {
		auto spawners = dZoneManager::Instance()->GetSpawnersByName(CounterweightSpawner);
		if (!spawners.empty()) {
			// Spawn the counter weight at a specific waypoint, there's one for each round
			auto* spawner = spawners.front();

			spawner->Spawn({
				spawner->m_Info.nodes.at((self->GetVar<uint32_t>(WaveNumberVariable) - 1) % 3)
				}, true);
		}
	} else if (timerName == LowerFrakjawCamTimer) {
		// Destroy the frakjaw on the ledge
		auto* ledgeFrakjaw = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(LedgeFrakjawVariable));
		if (ledgeFrakjaw != nullptr) {
			ledgeFrakjaw->Kill();
		}

		ActivityTimerStart(self, SpawnLowerFrakjawTimer, 1.0f, 1.0f);
		GameMessages::SendNotifyClientObject(self->GetObjectID(), PlayCinematicNotification, 0, 0,
			LWOOBJID_EMPTY, BottomFrakSpawn, UNASSIGNED_SYSTEM_ADDRESS);
	} else if (timerName == SpawnLowerFrakjawTimer) {
		auto spawners = dZoneManager::Instance()->GetSpawnersByName(LowerFrakjawSpawner);
		if (!spawners.empty()) {
			auto* spawner = spawners.front();
			spawner->Activate();
		}
	} else if (timerName == SpawnRailTimer) {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), PlayCinematicNotification, 0, 0,
			LWOOBJID_EMPTY, FireRailSpawn, UNASSIGNED_SYSTEM_ADDRESS);

		auto spawners = dZoneManager::Instance()->GetSpawnersByName(FireRailSpawner);
		if (!spawners.empty()) {
			auto* spawner = spawners.front();
			spawner->Activate();
		}
	} else if (timerName + TimerSplitChar == FrakjawSpawnInTimer) {
		auto* lowerFrakjaw = EntityManager::Instance()->GetEntity(objectID);
		if (lowerFrakjaw != nullptr) {
			LowerFrakjawSummon(self, lowerFrakjaw);
		}
	} else if (timerName == WaveOverTimer) {
		WaveOver(self);
	} else if (timerName == FightOverTimer) {
		FightOver(self);
	}
}

// // // // // // // //
// Custom functions  //
// // // // // // // //

void NjMonastryBossInstance::StartFight(Entity* self) {
	if (self->GetVar<bool>(FightStartedVariable))
		return;

	self->SetVar<bool>(FightStartedVariable, true);

	// Activate the frakjaw spawner
	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(LedgeFrakjawSpawner)) {
		spawner->Activate();
	}
}

void NjMonastryBossInstance::HandleLedgedFrakjawSpawned(Entity* self, Entity* ledgedFrakjaw) {
	self->SetVar<LWOOBJID>(LedgeFrakjawVariable, ledgedFrakjaw->GetObjectID());
	SummonWave(self, ledgedFrakjaw);
}

void NjMonastryBossInstance::HandleCounterWeightSpawned(Entity* self, Entity* counterWeight) {
	auto* rebuildComponent = counterWeight->GetComponent<RebuildComponent>();
	if (rebuildComponent != nullptr) {
		rebuildComponent->AddRebuildStateCallback([this, self, counterWeight](eRebuildState state) {

			switch (state) {
			case REBUILD_BUILDING:
				GameMessages::SendNotifyClientObject(self->GetObjectID(), PlayCinematicNotification,
					0, 0, counterWeight->GetObjectID(),
					BaseCounterweightQB + std::to_string(self->GetVar<uint32_t>(WaveNumberVariable)),
					UNASSIGNED_SYSTEM_ADDRESS);
				return;
			case REBUILD_INCOMPLETE:
				GameMessages::SendNotifyClientObject(self->GetObjectID(), EndCinematicNotification,
					0, 0, LWOOBJID_EMPTY, "",
					UNASSIGNED_SYSTEM_ADDRESS);
				return;
			case REBUILD_RESETTING:
				ActivityTimerStart(self, SpawnCounterWeightTimer, 0.0f, 0.0f);
				return;
			case REBUILD_COMPLETED: {
				// TODO: Move the platform?

				// The counterweight is actually a moving platform and we should listen to the last waypoint event here
				// 0.5f is a rough estimate of that path, though, and results in less needed logic
				self->AddCallbackTimer(0.5f, [this, self, counterWeight]() {
					if (counterWeight != nullptr) {
						counterWeight->Kill();
					}

					auto* frakjaw = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(LedgeFrakjawVariable));
					if (frakjaw == nullptr) {
						GameMessages::SendNotifyClientObject(self->GetObjectID(), u"LedgeFrakjawDead", 0,
							0, LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS);
						return;
					}

					auto* skillComponent = frakjaw->GetComponent<SkillComponent>();
					if (skillComponent != nullptr) {
						skillComponent->CalculateBehavior(1635, 39097, frakjaw->GetObjectID(), true, false);
					}

					GameMessages::SendPlayAnimation(frakjaw, StunnedAnimation);
					GameMessages::SendPlayNDAudioEmitter(frakjaw, UNASSIGNED_SYSTEM_ADDRESS, CounterSmashAudio);

					// Before wave 4 we should lower frakjaw from the ledge
					if (self->GetVar<uint32_t>(WaveNumberVariable) == 3) {
						LowerFrakjaw(self, frakjaw);
						return;
					}

					ActivityTimerStart(self, SpawnNextWaveTimer, 2.0f, 2.0f);
					});
			}
			default:
				return;
			}
			});
	}
}

void NjMonastryBossInstance::HandleLowerFrakjawSpawned(Entity* self, Entity* lowerFrakjaw) {
	GameMessages::SendPlayAnimation(lowerFrakjaw, TeleportInAnimation);
	self->SetVar<LWOOBJID>(LowerFrakjawVariable, lowerFrakjaw->GetObjectID());

	auto* combatAI = lowerFrakjaw->GetComponent<BaseCombatAIComponent>();
	if (combatAI != nullptr) {
		combatAI->SetDisabled(true);
	}

	auto* destroyableComponent = lowerFrakjaw->GetComponent<DestroyableComponent>();
	if (destroyableComponent != nullptr) {
		destroyableComponent->AddOnHitCallback([this, self, lowerFrakjaw](Entity* attacker) {
			NjMonastryBossInstance::HandleLowerFrakjawHit(self, lowerFrakjaw, attacker);
			});
	}

	lowerFrakjaw->AddDieCallback([this, self, lowerFrakjaw]() {
		NjMonastryBossInstance::HandleLowerFrakjawDied(self, lowerFrakjaw);
		});

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"LedgeFrakjawDead", 0, 0,
		LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS);

	if (self->GetVar<bool>(LargeTeamVariable)) {
		// Double frakjaws health for large teams
		if (destroyableComponent != nullptr) {
			const auto doubleHealth = destroyableComponent->GetHealth() * 2;
			destroyableComponent->SetHealth(doubleHealth);
			destroyableComponent->SetMaxHealth((float_t)doubleHealth);
		}

		ActivityTimerStart(self, FrakjawSpawnInTimer + std::to_string(lowerFrakjaw->GetObjectID()),
			2.0f, 2.0f);
		ActivityTimerStart(self, UnstunTimer + std::to_string(lowerFrakjaw->GetObjectID()),
			7.0f, 7.0f);
	} else {
		ActivityTimerStart(self, UnstunTimer + std::to_string(lowerFrakjaw->GetObjectID()),
			5.0f, 5.0f);
	}
}

void NjMonastryBossInstance::HandleLowerFrakjawHit(Entity* self, Entity* lowerFrakjaw, Entity* attacker) {
	auto* destroyableComponent = lowerFrakjaw->GetComponent<DestroyableComponent>();
	if (destroyableComponent == nullptr)
		return;

	// Progress the fight to the last wave if frakjaw has less than 50% of his health left
	if (destroyableComponent->GetHealth() <= (uint32_t)destroyableComponent->GetMaxHealth() / 2 && !self->GetVar<bool>(OnLastWaveVarbiale)) {
		self->SetVar<bool>(OnLastWaveVarbiale, true);

		// Stun frakjaw during the cinematic
		auto* combatAI = lowerFrakjaw->GetComponent<BaseCombatAIComponent>();
		if (combatAI != nullptr) {
			combatAI->SetDisabled(true);
		}
		ActivityTimerStart(self, UnstunTimer + std::to_string(lowerFrakjaw->GetObjectID()), 5.0f, 5.0f);

		const auto trashMobsAlive = self->GetVar<std::vector<LWOOBJID>>(TrashMobsAliveVariable);
		std::vector<LWOOBJID> newTrashMobs = {};

		for (const auto& trashMobID : trashMobsAlive) {
			auto* trashMob = EntityManager::Instance()->GetEntity(trashMobID);
			if (trashMob != nullptr) {
				newTrashMobs.push_back(trashMobID);

				// Stun all the enemies until the cinematic is over
				auto* trashMobCombatAI = trashMob->GetComponent<BaseCombatAIComponent>();
				if (trashMobCombatAI != nullptr) {
					trashMobCombatAI->SetDisabled(true);
				}
				ActivityTimerStart(self, UnstunTimer + std::to_string(trashMobID), 5.0f, 5.0f);
			}
		}

		self->SetVar<std::vector<LWOOBJID>>(TrashMobsAliveVariable, newTrashMobs);

		LowerFrakjawSummon(self, lowerFrakjaw);
		RemovePoison(self);
	}
}

void NjMonastryBossInstance::HandleLowerFrakjawDied(Entity* self, Entity* lowerFrakjaw) {
	ActivityTimerStart(self, FightOverTimer, 2.0f, 2.0f);
}

void NjMonastryBossInstance::HandleWaveEnemySpawned(Entity* self, Entity* waveEnemy) {
	waveEnemy->AddDieCallback([this, self, waveEnemy]() {
		NjMonastryBossInstance::HandleWaveEnemyDied(self, waveEnemy);
		});

	auto waveEnemies = self->GetVar<std::vector<LWOOBJID>>(TrashMobsAliveVariable);
	waveEnemies.push_back(waveEnemy->GetObjectID());
	self->SetVar<std::vector<LWOOBJID>>(TrashMobsAliveVariable, waveEnemies);

	auto* combatAI = waveEnemy->GetComponent<BaseCombatAIComponent>();
	if (combatAI != nullptr) {
		combatAI->SetDisabled(true);
		ActivityTimerStart(self, UnstunTimer + std::to_string(waveEnemy->GetObjectID()), 3.0f, 3.0f);
	}
}

void NjMonastryBossInstance::HandleWaveEnemyDied(Entity* self, Entity* waveEnemy) {
	auto waveEnemies = self->GetVar<std::vector<LWOOBJID>>(TrashMobsAliveVariable);
	waveEnemies.erase(std::remove(waveEnemies.begin(), waveEnemies.end(), waveEnemy->GetObjectID()), waveEnemies.end());
	self->SetVar<std::vector<LWOOBJID>>(TrashMobsAliveVariable, waveEnemies);

	if (waveEnemies.empty()) {
		ActivityTimerStart(self, WaveOverTimer, 2.0f, 2.0f);
	}
}

void NjMonastryBossInstance::TeleportPlayer(Entity* player, uint32_t position) {
	for (const auto* spawnPoint : EntityManager::Instance()->GetEntitiesInGroup("SpawnPoint" + std::to_string(position))) {
		GameMessages::SendTeleport(player->GetObjectID(), spawnPoint->GetPosition(), spawnPoint->GetRotation(),
			player->GetSystemAddress(), true);
	}
}

void NjMonastryBossInstance::SummonWave(Entity* self, Entity* frakjaw) {
	GameMessages::SendNotifyClientObject(self->GetObjectID(), PlayCinematicNotification, 0, 0, LWOOBJID_EMPTY,
		LedgeFrakSummon, UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::SendPlayAnimation(frakjaw, SummonAnimation);

	// Stop the music for the first, fourth and fifth wave
	const auto wave = self->GetVar<uint32_t>(WaveNumberVariable);
	if (wave >= 1 || wave < (m_Waves.size() - 1)) {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), StopMusicNotification, 0, 0,
			LWOOBJID_EMPTY, AudioWaveAudio + std::to_string(wave - 1),
			UNASSIGNED_SYSTEM_ADDRESS);
	}

	// After frakjaw moves down the music stays the same
	if (wave < (m_Waves.size() - 1)) {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), StartMusicNotification, 0, 0,
			LWOOBJID_EMPTY, AudioWaveAudio + std::to_string(wave),
			UNASSIGNED_SYSTEM_ADDRESS);
	}

	ActivityTimerStart(self, SpawnWaveTimer, 4.0f, 4.0f);
}

void NjMonastryBossInstance::LowerFrakjawSummon(Entity* self, Entity* frakjaw) {
	GameMessages::SendNotifyClientObject(self->GetObjectID(), PlayCinematicNotification, 0, 0,
		LWOOBJID_EMPTY, BottomFrakSummon, UNASSIGNED_SYSTEM_ADDRESS);
	ActivityTimerStart(self, SpawnWaveTimer, 2.0f, 2.0f);
	GameMessages::SendPlayAnimation(frakjaw, SummonAnimation);
}

void NjMonastryBossInstance::RemovePoison(Entity* self) {
	const auto& totalPlayer = self->GetVar<std::vector<LWOOBJID>>(TotalPlayersLoadedVariable);
	for (const auto& playerID : totalPlayer) {

		auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player != nullptr) {

			auto* buffComponent = player->GetComponent<BuffComponent>();
			if (buffComponent != nullptr) {
				buffComponent->RemoveBuff(PoisonBuff);
			}
		}
	}
}

void NjMonastryBossInstance::LowerFrakjaw(Entity* self, Entity* frakjaw) {
	GameMessages::SendPlayAnimation(frakjaw, TeleportOutAnimation);
	ActivityTimerStart(self, LowerFrakjawCamTimer, 2.0f, 2.0f);

	GameMessages::SendNotifyClientObject(frakjaw->GetObjectID(), StopMusicNotification, 0, 0,
		LWOOBJID_EMPTY, AudioWaveAudio + std::to_string(m_Waves.size() - 3), UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::SendNotifyClientObject(frakjaw->GetObjectID(), StartMusicNotification, 0, 0,
		LWOOBJID_EMPTY, AudioWaveAudio + std::to_string(m_Waves.size() - 2), UNASSIGNED_SYSTEM_ADDRESS);
}

void NjMonastryBossInstance::SpawnOnNetwork(Entity* self, const LOT& toSpawn, const uint32_t& numberToSpawn, const std::string& spawnerName) {
	auto spawners = dZoneManager::Instance()->GetSpawnersByName(spawnerName);
	if (spawners.empty() || numberToSpawn <= 0)
		return;

	auto* spawner = spawners.front();

	// Spawn the lot N times
	spawner->SetSpawnLot(toSpawn);
	for (auto i = 0; i < numberToSpawn; i++)
		spawner->Spawn({ spawner->m_Info.nodes.at(i % spawner->m_Info.nodes.size()) }, true);
}

void NjMonastryBossInstance::WaveOver(Entity* self) {
	auto wave = self->GetVar<uint32_t>(WaveNumberVariable);
	if (wave >= m_Waves.size() - 1)
		return;

	GameMessages::SendNotifyClientObject(self->GetObjectID(), PlayCinematicNotification, 0, 0,
		LWOOBJID_EMPTY, BaseCounterweightSpawn + std::to_string(wave),
		UNASSIGNED_SYSTEM_ADDRESS);
	ActivityTimerStart(self, SpawnCounterWeightTimer, 1.5f, 1.5f);
	RemovePoison(self);
}

void NjMonastryBossInstance::FightOver(Entity* self) {
	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"GroundFrakjawDead", 0, 0,
		LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS);

	// Remove all the enemies from the battlefield
	for (auto i = 1; i < 5; i++) {
		auto spawners = dZoneManager::Instance()->GetSpawnersByName(BaseEnemiesSpawner + std::to_string(i));
		if (!spawners.empty()) {
			auto* spawner = spawners.front();
			spawner->Deactivate();
			spawner->Reset();
		}
	}

	RemovePoison(self);
	ActivityTimerStart(self, SpawnRailTimer, 1.5f, 1.5f);

	// Set the music to play the victory music
	GameMessages::SendNotifyClientObject(self->GetObjectID(), StopMusicNotification, 0, 0,
		LWOOBJID_EMPTY, AudioWaveAudio + std::to_string(m_Waves.size() - 2),
		UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::SendNotifyClientObject(self->GetObjectID(), FlashMusicNotification, 0, 0,
		LWOOBJID_EMPTY, "Monastery_Frakjaw_Battle_Win", UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::SendNotifyClientObject(self->GetObjectID(), PlayCinematicNotification, 0, 0,
		LWOOBJID_EMPTY, TreasureChestSpawning, UNASSIGNED_SYSTEM_ADDRESS);

	auto treasureChests = EntityManager::Instance()->GetEntitiesInGroup(ChestSpawnpointGroup);
	for (auto* treasureChest : treasureChests) {
		auto info = EntityInfo{};

		info.lot = ChestLOT;
		info.pos = treasureChest->GetPosition();
		info.rot = treasureChest->GetRotation();
		info.spawnerID = self->GetObjectID();
		info.settings = {
			new LDFData<LWOOBJID>(u"parent_tag", self->GetObjectID())
		};

		// Finally spawn a treasure chest at the correct spawn point
		auto* chestObject = EntityManager::Instance()->CreateEntity(info);
		EntityManager::Instance()->ConstructEntity(chestObject);
	}
}
