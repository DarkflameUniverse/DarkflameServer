/**
 * Thanks to Simon for his early research on the racing system.
 */

#include "RacingControlComponent.h"

#include "CharacterComponent.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "MissionComponent.h"
#include "ModuleAssemblyComponent.h"
#include "Player.h"
#include "PossessableComponent.h"
#include "PossessorComponent.h"
#include "eRacingTaskParam.h"
#include "Spawner.h"
#include "VehiclePhysicsComponent.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "dConfig.h"
#include "Loot.h"
#include "eMissionTaskType.h"
#include "dZoneManager.h"
#include "CDActivitiesTable.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

RacingControlComponent::RacingControlComponent(Entity* parent)
	: Component(parent) {
	m_PathName = u"MainPath";
	m_RemainingLaps = 3;
	m_LeadingPlayer = LWOOBJID_EMPTY;
	m_RaceBestTime = 0;
	m_RaceBestLap = 0;
	m_Started = false;
	m_StartTimer = 0;
	m_Loaded = false;
	m_LoadedPlayers = 0;
	m_LoadTimer = 0;
	m_Finished = 0;
	m_StartTime = 0;
	m_EmptyTimer = 0;
	m_SoloRacing = Game::config->GetValue("solo_racing") == "1";

	m_MainWorld = 1200;
	const auto worldID = Game::server->GetZoneID();
	if (Game::zoneManager->CheckIfAccessibleZone((worldID/10)*10)) m_MainWorld = (worldID/10)*10;

	m_ActivityID = 42;
	CDActivitiesTable* activitiesTable = CDClientManager::Instance().GetTable<CDActivitiesTable>();
	std::vector<CDActivities> activities = activitiesTable->Query([=](CDActivities entry) {return (entry.instanceMapID == worldID); });
	for (CDActivities activity : activities) m_ActivityID = activity.ActivityID;
}

RacingControlComponent::~RacingControlComponent() {}

void RacingControlComponent::OnPlayerLoaded(Entity* player) {
	// If the race has already started, send the player back to the main world.
	if (m_Loaded) {
		auto* playerInstance = dynamic_cast<Player*>(player);

		playerInstance->SendToZone(m_MainWorld);

		return;
	}

	const auto objectID = player->GetObjectID();

	m_LoadedPlayers++;

	Game::logger->Log("RacingControlComponent", "Loading player %i",
		m_LoadedPlayers);

	m_LobbyPlayers.push_back(objectID);
}

void RacingControlComponent::LoadPlayerVehicle(Entity* player,
	uint32_t positionNumber, bool initialLoad) {
	// Load the player's vehicle.

	if (player == nullptr) {
		return;
	}

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	// Find the player's vehicle.

	auto* item = inventoryComponent->FindItemByLot(8092);

	if (item == nullptr) {
		Game::logger->Log("RacingControlComponent", "Failed to find item");

		return;
	}

	// Calculate the vehicle's starting position.

	auto* path = Game::zoneManager->GetZone()->GetPath(
		GeneralUtils::UTF16ToWTF8(m_PathName));

	auto spawnPointEntities = Game::entityManager->GetEntitiesByLOT(4843);
	auto startPosition = NiPoint3::ZERO;
	auto startRotation = NiQuaternion::IDENTITY;
	const std::string placementAsString = std::to_string(positionNumber);
	for (auto entity : spawnPointEntities) {
		if (!entity) continue;
		if (entity->GetVarAsString(u"placement") == placementAsString) {
			startPosition = entity->GetPosition();
			startRotation = entity->GetRotation();
			break;
		}
	}

	// Make sure the player is at the correct position.

	GameMessages::SendTeleport(player->GetObjectID(), startPosition,
		startRotation, player->GetSystemAddress(), true);

	// Spawn the vehicle entity.

	EntityInfo info{};
	info.lot = 8092;
	info.pos = startPosition;
	info.rot = startRotation;
	info.spawnerID = m_Parent->GetObjectID();

	auto* carEntity =
		Game::entityManager->CreateEntity(info, nullptr, m_Parent);

	// Make the vehicle a child of the racing controller.
	m_Parent->AddChild(carEntity);

	auto* destroyableComponent =
		carEntity->GetComponent<DestroyableComponent>();

	// Setup the vehicle stats.
	if (destroyableComponent != nullptr) {
		destroyableComponent->SetMaxImagination(60);
		destroyableComponent->SetImagination(0);
	}

	// Setup the vehicle as being possessed by the player.
	auto* possessableComponent =
		carEntity->GetComponent<PossessableComponent>();

	if (possessableComponent != nullptr) {
		possessableComponent->SetPossessor(player->GetObjectID());
	}

	// Load the vehicle's assemblyPartLOTs for display.
	auto* moduleAssemblyComponent =
		carEntity->GetComponent<ModuleAssemblyComponent>();

	if (moduleAssemblyComponent) {
		moduleAssemblyComponent->SetSubKey(item->GetSubKey());
		moduleAssemblyComponent->SetUseOptionalParts(false);

		for (auto* config : item->GetConfig()) {
			if (config->GetKey() == u"assemblyPartLOTs") {
				moduleAssemblyComponent->SetAssemblyPartsLOTs(
					GeneralUtils::ASCIIToUTF16(config->GetValueAsString()));
			}
		}
	}

	// Setup the player as possessing the vehicle.
	auto* possessorComponent = player->GetComponent<PossessorComponent>();

	if (possessorComponent != nullptr) {
		possessorComponent->SetPossessable(carEntity->GetObjectID());
		possessorComponent->SetPossessableType(ePossessionType::ATTACHED_VISIBLE); // for racing it's always Attached_Visible
	}

	// Set the player's current activity as racing.
	auto* characterComponent = player->GetComponent<CharacterComponent>();

	if (characterComponent != nullptr) {
		characterComponent->SetIsRacing(true);
	}

	// Init the player's racing entry.
	if (initialLoad) {
		m_RacingPlayers.push_back(
			{ player->GetObjectID(),
			 carEntity->GetObjectID(),
			 static_cast<uint32_t>(m_RacingPlayers.size()),
			 false,
			 {},
			 startPosition,
			 startRotation,
			 0,
			 0,
			 0,
			 0 });
	}

	// Construct and serialize everything when done.

	Game::entityManager->ConstructEntity(carEntity);
	Game::entityManager->SerializeEntity(player);
	Game::entityManager->SerializeEntity(m_Parent);

	GameMessages::SendRacingSetPlayerResetInfo(
		m_Parent->GetObjectID(), 0, 0, player->GetObjectID(), startPosition, 1,
		UNASSIGNED_SYSTEM_ADDRESS);

	const auto playerID = player->GetObjectID();

	// Reset the player to the start position during downtime, in case something
	// went wrong.
	m_Parent->AddCallbackTimer(1, [this, playerID]() {
		auto* player = Game::entityManager->GetEntity(playerID);

		if (player == nullptr) {
			return;
		}

		GameMessages::SendRacingResetPlayerToLastReset(
			m_Parent->GetObjectID(), playerID, UNASSIGNED_SYSTEM_ADDRESS);
		});

	GameMessages::SendSetJetPackMode(player, false);

	// Set the vehicle's state.
	GameMessages::SendNotifyVehicleOfRacingObject(carEntity->GetObjectID(),
		m_Parent->GetObjectID(),
		UNASSIGNED_SYSTEM_ADDRESS);

	GameMessages::SendVehicleSetWheelLockState(carEntity->GetObjectID(), false,
		initialLoad,
		UNASSIGNED_SYSTEM_ADDRESS);

	// Make sure everything has the correct position.
	GameMessages::SendTeleport(player->GetObjectID(), startPosition,
		startRotation, player->GetSystemAddress(), true);
	GameMessages::SendTeleport(carEntity->GetObjectID(), startPosition,
		startRotation, player->GetSystemAddress(), true);
}

void RacingControlComponent::OnRacingClientReady(Entity* player) {
	// Notify the other players that this player is ready.

	for (auto& racingPlayer : m_RacingPlayers) {
		if (racingPlayer.playerID != player->GetObjectID()) {
			if (racingPlayer.playerLoaded) {
				GameMessages::SendRacingPlayerLoaded(
					m_Parent->GetObjectID(), racingPlayer.playerID,
					racingPlayer.vehicleID, UNASSIGNED_SYSTEM_ADDRESS);
			}

			continue;
		}

		racingPlayer.playerLoaded = true;

		GameMessages::SendRacingPlayerLoaded(
			m_Parent->GetObjectID(), racingPlayer.playerID,
			racingPlayer.vehicleID, UNASSIGNED_SYSTEM_ADDRESS);
	}

	Game::entityManager->SerializeEntity(m_Parent);
}

void RacingControlComponent::OnRequestDie(Entity* player) {
	// Sent by the client when they collide with something which should smash
	// them.

	for (auto& racingPlayer : m_RacingPlayers) {
		if (racingPlayer.playerID != player->GetObjectID()) {
			continue;
		}

		auto* vehicle =
			Game::entityManager->GetEntity(racingPlayer.vehicleID);

		if (!vehicle) return;

		if (!racingPlayer.noSmashOnReload) {
			racingPlayer.smashedTimes++;
			GameMessages::SendDie(vehicle, vehicle->GetObjectID(), LWOOBJID_EMPTY, true,
				eKillType::VIOLENT, u"", 0, 0, 90.0f, false, true, 0);

			auto* destroyableComponent = vehicle->GetComponent<DestroyableComponent>();
			uint32_t respawnImagination = 0;
			// Reset imagination to half its current value, rounded up to the nearest value divisible by 10, as it was done in live.
			// Do not actually change the value yet.  Do that on respawn.
			if (destroyableComponent) {
				respawnImagination = static_cast<int32_t>(ceil(destroyableComponent->GetImagination() / 2.0f / 10.0f)) * 10.0f;
				GameMessages::SendSetResurrectRestoreValues(vehicle, -1, -1, respawnImagination);
			}

			// Respawn the player in 2 seconds, as was done in live.  Not sure if this value is in a setting somewhere else...
			vehicle->AddCallbackTimer(2.0f, [=]() {
				if (!vehicle || !this->m_Parent) return;
				GameMessages::SendRacingResetPlayerToLastReset(
					m_Parent->GetObjectID(), racingPlayer.playerID,
					UNASSIGNED_SYSTEM_ADDRESS);

				GameMessages::SendVehicleStopBoost(vehicle, player->GetSystemAddress(), true);

				GameMessages::SendRacingSetPlayerResetInfo(
					m_Parent->GetObjectID(), racingPlayer.lap,
					racingPlayer.respawnIndex, player->GetObjectID(),
					racingPlayer.respawnPosition, racingPlayer.respawnIndex + 1,
					UNASSIGNED_SYSTEM_ADDRESS);

				GameMessages::SendResurrect(vehicle);
				auto* destroyableComponent = vehicle->GetComponent<DestroyableComponent>();
				// Reset imagination to half its current value, rounded up to the nearest value divisible by 10, as it was done in live.
				if (destroyableComponent) destroyableComponent->SetImagination(respawnImagination);
				Game::entityManager->SerializeEntity(vehicle);
			});

			auto* characterComponent = player->GetComponent<CharacterComponent>();
			if (characterComponent != nullptr) {
				characterComponent->UpdatePlayerStatistic(RacingTimesWrecked);
			}
		} else {
			GameMessages::SendRacingSetPlayerResetInfo(
				m_Parent->GetObjectID(), racingPlayer.lap,
				racingPlayer.respawnIndex, player->GetObjectID(),
				racingPlayer.respawnPosition, racingPlayer.respawnIndex + 1,
				UNASSIGNED_SYSTEM_ADDRESS);
			GameMessages::SendRacingResetPlayerToLastReset(
				m_Parent->GetObjectID(), racingPlayer.playerID,
				UNASSIGNED_SYSTEM_ADDRESS);
		}
	}
}

void RacingControlComponent::OnRacingPlayerInfoResetFinished(Entity* player) {
	// When the player has respawned.

	for (auto& racingPlayer : m_RacingPlayers) {
		if (racingPlayer.playerID != player->GetObjectID()) {
			continue;
		}

		auto* vehicle =
			Game::entityManager->GetEntity(racingPlayer.vehicleID);

		if (vehicle == nullptr) {
			return;
		}

		racingPlayer.noSmashOnReload = false;

		return;
	}
}

void RacingControlComponent::HandleMessageBoxResponse(Entity* player, int32_t button, const std::string& id) {
	auto* data = GetPlayerData(player->GetObjectID());

	if (data == nullptr) {
		return;
	}

	if (id == "rewardButton") {
		if (data->collectedRewards) {
			return;
		}

		data->collectedRewards = true;

		// Calculate the score, different loot depending on player count
		const auto score = m_LoadedPlayers * 10 + data->finished;

		LootGenerator::Instance().GiveActivityLoot(player, m_Parent, m_ActivityID, score);

		// Giving rewards
		GameMessages::SendNotifyRacingClient(
			m_Parent->GetObjectID(), 2, 0, LWOOBJID_EMPTY, u"",
			player->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);

		auto* missionComponent = player->GetComponent<MissionComponent>();

		if (missionComponent == nullptr) return;

		missionComponent->Progress(eMissionTaskType::RACING, 0, (LWOOBJID)eRacingTaskParam::COMPETED_IN_RACE); // Progress task for competing in a race
		missionComponent->Progress(eMissionTaskType::RACING, data->smashedTimes, (LWOOBJID)eRacingTaskParam::SAFE_DRIVER); // Finish a race without being smashed.

		// If solo racing is enabled OR if there are 3 players in the race, progress placement tasks.
		if (m_SoloRacing || m_LoadedPlayers > 2) {
			missionComponent->Progress(eMissionTaskType::RACING, data->finished, (LWOOBJID)eRacingTaskParam::FINISH_WITH_PLACEMENT); // Finish in 1st place on a race
			if (data->finished == 1) {
				missionComponent->Progress(eMissionTaskType::RACING, Game::zoneManager->GetZone()->GetWorldID(), (LWOOBJID)eRacingTaskParam::FIRST_PLACE_MULTIPLE_TRACKS); // Finish in 1st place on multiple tracks.
				missionComponent->Progress(eMissionTaskType::RACING, Game::zoneManager->GetZone()->GetWorldID(), (LWOOBJID)eRacingTaskParam::WIN_RACE_IN_WORLD); // Finished first place in specific world.
			}
			if (data->finished == m_LoadedPlayers) {
				missionComponent->Progress(eMissionTaskType::RACING, Game::zoneManager->GetZone()->GetWorldID(), (LWOOBJID)eRacingTaskParam::LAST_PLACE_FINISH); // Finished first place in specific world.
			}
		}
	} else if ((id == "ACT_RACE_EXIT_THE_RACE?" || id == "Exit") && button == m_ActivityExitConfirm) {
		auto* vehicle = Game::entityManager->GetEntity(data->vehicleID);

		if (vehicle == nullptr) {
			return;
		}

		// Exiting race
		GameMessages::SendNotifyRacingClient(
			m_Parent->GetObjectID(), 3, 0, LWOOBJID_EMPTY, u"",
			player->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);

		auto* playerInstance = dynamic_cast<Player*>(player);

		playerInstance->SendToZone(m_MainWorld);

		vehicle->Kill();
	}
}

void RacingControlComponent::Serialize(RakNet::BitStream* outBitStream,
	bool bIsInitialUpdate,
	unsigned int& flags) {
	// BEGIN Scripted Activity

	outBitStream->Write1();

	outBitStream->Write(static_cast<uint32_t>(m_RacingPlayers.size()));
	for (const auto& player : m_RacingPlayers) {
		outBitStream->Write(player.playerID);

		for (int i = 0; i < 10; i++) {
			outBitStream->Write(player.data[i]);
		}
	}

	// END Scripted Activity

	outBitStream->Write1(); // Dirty?
	outBitStream->Write(static_cast<uint16_t>(m_RacingPlayers.size()));

	outBitStream->Write(!m_RacingPlayers.empty());
	if (!m_RacingPlayers.empty()) {
		for (const auto& player : m_RacingPlayers) {
			outBitStream->Write1(); // Has more date

			outBitStream->Write(player.playerID);
			outBitStream->Write(player.vehicleID);
			outBitStream->Write(player.playerIndex);
			outBitStream->Write(player.playerLoaded);
		}

		outBitStream->Write0(); // No more data
	}

	outBitStream->Write(!m_RacingPlayers.empty());
	if (!m_RacingPlayers.empty()) {
		for (const auto& player : m_RacingPlayers) {
			outBitStream->Write1(); // Has more date

			outBitStream->Write(player.playerID);
			outBitStream->Write<uint32_t>(0);
		}

		outBitStream->Write0(); // No more data
	}

	outBitStream->Write1(); // Dirty?

	outBitStream->Write(m_RemainingLaps);

	outBitStream->Write(static_cast<uint16_t>(m_PathName.size()));
	for (const auto character : m_PathName) {
		outBitStream->Write(character);
	}

	outBitStream->Write1(); // ???
	outBitStream->Write1(); // ???

	outBitStream->Write(m_LeadingPlayer);
	outBitStream->Write(m_RaceBestLap);
	outBitStream->Write(m_RaceBestTime);
}

RacingPlayerInfo* RacingControlComponent::GetPlayerData(LWOOBJID playerID) {
	for (auto& player : m_RacingPlayers) {
		if (player.playerID == playerID) {
			return &player;
		}
	}

	return nullptr;
}

void RacingControlComponent::Update(float deltaTime) {
	// This method is a mess.

	// Pre-load routine
	if (!m_Loaded) {
		// Check if any players has disconnected before loading in
		for (size_t i = 0; i < m_LobbyPlayers.size(); i++) {
			auto* playerEntity =
				Game::entityManager->GetEntity(m_LobbyPlayers[i]);

			if (playerEntity == nullptr) {
				--m_LoadedPlayers;

				m_LobbyPlayers.erase(m_LobbyPlayers.begin() + i);

				return;
			}
		}

		if (m_LoadedPlayers >= 2 || (m_LoadedPlayers == 1 && m_SoloRacing)) {
			m_LoadTimer += deltaTime;
		} else {
			m_EmptyTimer += deltaTime;
		}

		// If a player happens to be left alone for more then 30 seconds without
		// anyone else loading in, send them back to the main world
		if (m_EmptyTimer >= 30) {
			for (const auto player : m_LobbyPlayers) {
				auto* playerEntity =
					Game::entityManager->GetEntity(player);

				if (playerEntity == nullptr) {
					continue;
				}

				auto* playerInstance = dynamic_cast<Player*>(playerEntity);

				playerInstance->SendToZone(m_MainWorld);
			}

			m_LobbyPlayers.clear();
		}

		// From the first 2 players loading in the rest have a max of 15 seconds
		// to load in, can raise this if it's too low
		if (m_LoadTimer >= 15) {
			Game::logger->Log("RacingControlComponent",
				"Loading all players...");

			for (size_t positionNumber = 0; positionNumber < m_LobbyPlayers.size(); positionNumber++) {
				Game::logger->Log("RacingControlComponent",
					"Loading player now!");

				auto* player =
					Game::entityManager->GetEntity(m_LobbyPlayers[positionNumber]);

				if (player == nullptr) {
					return;
				}

				Game::logger->Log("RacingControlComponent",
					"Loading player now NOW!");

				LoadPlayerVehicle(player, positionNumber + 1, true);

				m_Loaded = true;
			}

			m_Loaded = true;
		}

		return;
	}

	// The players who will be participating have loaded
	if (!m_Started) {
		// Check if anyone has disconnected during this period
		for (size_t i = 0; i < m_RacingPlayers.size(); i++) {
			auto* playerEntity = Game::entityManager->GetEntity(
				m_RacingPlayers[i].playerID);

			if (playerEntity == nullptr) {
				m_RacingPlayers.erase(m_RacingPlayers.begin() + i);

				--m_LoadedPlayers;

				return;
			}
		}

		// If less then 2 players are left, send the rest back to the main world
		if (m_LoadedPlayers < 2 && !(m_LoadedPlayers == 1 && m_SoloRacing)) {
			for (const auto player : m_LobbyPlayers) {
				auto* playerEntity =
					Game::entityManager->GetEntity(player);

				if (playerEntity == nullptr) {
					continue;
				}

				auto* playerInstance = dynamic_cast<Player*>(playerEntity);

				playerInstance->SendToZone(m_MainWorld);
			}

			return;
		}

		// Check if all players have send a ready message

		int32_t readyPlayers = 0;

		for (const auto& player : m_RacingPlayers) {
			if (player.playerLoaded) {
				++readyPlayers;
			}
		}

		if (readyPlayers >= m_LoadedPlayers) {
			// Setup for racing
			if (m_StartTimer == 0) {
				GameMessages::SendNotifyRacingClient(
					m_Parent->GetObjectID(), 1, 0, LWOOBJID_EMPTY, u"",
					LWOOBJID_EMPTY, UNASSIGNED_SYSTEM_ADDRESS);

				for (const auto& player : m_RacingPlayers) {
					auto* vehicle =
						Game::entityManager->GetEntity(player.vehicleID);
					auto* playerEntity =
						Game::entityManager->GetEntity(player.playerID);

					if (vehicle != nullptr && playerEntity != nullptr) {
						GameMessages::SendTeleport(
							player.playerID, player.respawnPosition,
							player.respawnRotation,
							playerEntity->GetSystemAddress(), true);

						vehicle->SetPosition(player.respawnPosition);
						vehicle->SetRotation(player.respawnRotation);

						auto* destroyableComponent =
							vehicle->GetComponent<DestroyableComponent>();

						if (destroyableComponent != nullptr) {
							destroyableComponent->SetImagination(0);
						}

						Game::entityManager->SerializeEntity(vehicle);
						Game::entityManager->SerializeEntity(
							playerEntity);
					}
				}

				// Spawn imagination pickups
				auto* minSpawner = Game::zoneManager->GetSpawnersByName(
					"ImaginationSpawn_Min")[0];
				auto* medSpawner = Game::zoneManager->GetSpawnersByName(
					"ImaginationSpawn_Med")[0];
				auto* maxSpawner = Game::zoneManager->GetSpawnersByName(
					"ImaginationSpawn_Max")[0];

				minSpawner->Activate();

				if (m_LoadedPlayers > 2) {
					medSpawner->Activate();
				}

				if (m_LoadedPlayers > 4) {
					maxSpawner->Activate();
				}

				// Reset players to their start location, without smashing them
				for (auto& player : m_RacingPlayers) {
					auto* vehicleEntity =
						Game::entityManager->GetEntity(player.vehicleID);
					auto* playerEntity =
						Game::entityManager->GetEntity(player.playerID);

					if (vehicleEntity == nullptr || playerEntity == nullptr) {
						continue;
					}

					player.noSmashOnReload = true;

					OnRequestDie(playerEntity);
				}
			}
			// This 6 seconds seems to be hardcoded in the client, start race
			// after that amount of time
			else if (m_StartTimer >= 6) {
				// Activate the players movement
				for (auto& player : m_RacingPlayers) {
					auto* vehicleEntity =
						Game::entityManager->GetEntity(player.vehicleID);
					auto* playerEntity =
						Game::entityManager->GetEntity(player.playerID);

					if (vehicleEntity == nullptr || playerEntity == nullptr) {
						continue;
					}

					GameMessages::SendVehicleUnlockInput(
						player.vehicleID, false, UNASSIGNED_SYSTEM_ADDRESS);
				}

				// Start the race
				GameMessages::SendActivityStart(m_Parent->GetObjectID(),
					UNASSIGNED_SYSTEM_ADDRESS);

				m_Started = true;

				Game::logger->Log("RacingControlComponent", "Starting race");

				Game::entityManager->SerializeEntity(m_Parent);

				m_StartTime = std::time(nullptr);
			}

			m_StartTimer += deltaTime;
		} else {
			m_StartTimer = 0;
		}

		return;
	}

	// Race routines
	auto* path = Game::zoneManager->GetZone()->GetPath(
		GeneralUtils::UTF16ToWTF8(m_PathName));

	for (auto& player : m_RacingPlayers) {
		auto* vehicle = Game::entityManager->GetEntity(player.vehicleID);
		auto* playerEntity =
			Game::entityManager->GetEntity(player.playerID);

		if (vehicle == nullptr || playerEntity == nullptr) {
			continue;
		}

		const auto vehiclePosition = vehicle->GetPosition();

		// If the player is this far below the map, safe to assume they should
		// be smashed by death plane
		if (vehiclePosition.y < -500) {
			GameMessages::SendDie(vehicle, m_Parent->GetObjectID(),
				LWOOBJID_EMPTY, true, eKillType::VIOLENT, u"", 0, 0, 0,
				true, false, 0);

			OnRequestDie(playerEntity);

			continue;
		}

		// Loop through all the waypoints and see if the player has reached a
		// new checkpoint
		uint32_t respawnIndex = 0;
		for (const auto& waypoint : path->pathWaypoints) {
			if (player.lap == 3) {
				break;
			}

			if (player.respawnIndex == respawnIndex) {
				++respawnIndex;

				continue;
			}

			const auto& position = waypoint.position;

			if (std::abs((int)respawnIndex - (int)player.respawnIndex) > 10 &&
				player.respawnIndex != path->pathWaypoints.size() - 1) {
				++respawnIndex;

				continue;
			}

			if (Vector3::DistanceSquared(position, vehiclePosition) > 50 * 50) {
				++respawnIndex;

				continue;
			}

			// Only go upwards, except if we've lapped
			// Not sure how we are supposed to check if they've reach a
			// checkpoint, within 50 units seems safe
			if (!(respawnIndex > player.respawnIndex ||
				player.respawnIndex == path->pathWaypoints.size() - 1)) {
				++respawnIndex;

				continue;
			}

			// Some offset up to make they don't fall through the terrain on a
			// respawn, seems to fix itself to the track anyhow
			player.respawnPosition = position + NiPoint3::UNIT_Y * 5;
			player.respawnRotation = vehicle->GetRotation();
			player.respawnIndex = respawnIndex;

			// Reached the start point, lapped
			if (respawnIndex == 0) {
				time_t lapTime = std::time(nullptr) - (player.lap == 0 ? m_StartTime : player.lapTime);

				// Cheating check
				if (lapTime < 40) {
					continue;
				}

				player.lap++;

				player.lapTime = std::time(nullptr);

				if (player.bestLapTime == 0 || player.bestLapTime > lapTime) {
					player.bestLapTime = lapTime;

					Game::logger->Log("RacingControlComponent",
						"Best lap time (%llu)", lapTime);
				}

				auto* missionComponent =
					playerEntity->GetComponent<MissionComponent>();

				if (missionComponent != nullptr) {

					// Progress lap time tasks
					missionComponent->Progress(eMissionTaskType::RACING, (lapTime) * 1000, (LWOOBJID)eRacingTaskParam::LAP_TIME);

					if (player.lap == 3) {
						m_Finished++;
						player.finished = m_Finished;

						const auto raceTime =
							(std::time(nullptr) - m_StartTime);

						player.raceTime = raceTime;

						Game::logger->Log("RacingControlComponent",
							"Completed time %llu, %llu",
							raceTime, raceTime * 1000);

						// Entire race time
						missionComponent->Progress(eMissionTaskType::RACING, (raceTime) * 1000, (LWOOBJID)eRacingTaskParam::TOTAL_TRACK_TIME);

						auto* characterComponent = playerEntity->GetComponent<CharacterComponent>();
						if (characterComponent != nullptr) {
							characterComponent->TrackRaceCompleted(m_Finished == 1);
						}

						// TODO: Figure out how to update the GUI leaderboard.
					}
				}

				Game::logger->Log("RacingControlComponent",
					"Lapped (%i) in (%llu)", player.lap,
					lapTime);
			}

			Game::logger->Log("RacingControlComponent",
				"Reached point (%i)/(%i)", player.respawnIndex,
				path->pathWaypoints.size());

			break;
		}
	}
}

std::string RacingControlComponent::FormatTimeString(time_t time) {
	int32_t min = time / 60;
	time -= min * 60;
	int32_t sec = time;

	std::string minText;
	std::string secText;

	if (min <= 0) {
		minText = "0";
	} else {
		minText = std::to_string(min);
	}

	if (sec <= 0) {
		secText = "00";
	} else if (sec <= 9) {
		secText = "0" + std::to_string(sec);
	} else {
		secText = std::to_string(sec);
	}

	return minText + ":" + secText + ".00";
}
