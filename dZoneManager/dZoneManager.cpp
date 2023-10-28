#include "Game.h"
#include "dCommonVars.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "Logger.h"
#include "dConfig.h"
#include "InventoryComponent.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "VanityUtilities.h"
#include "WorldConfig.h"
#include "CDZoneTableTable.h"
#include <chrono>
#include "eObjectBits.h"
#include "CDZoneTableTable.h"
#include "AssetManager.h"

#include "../dWorldServer/ObjectIDManager.h"

void dZoneManager::Initialize(const LWOZONEID& zoneID) {
	LOG("Preparing zone: %i/%i/%i", zoneID.GetMapID(), zoneID.GetInstanceID(), zoneID.GetCloneID());

	int64_t startTime = 0;
	int64_t endTime = 0;

	startTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

	LoadZone(zoneID);

	LOT zoneControlTemplate = 2365;

	CDZoneTableTable* zoneTable = CDClientManager::Instance().GetTable<CDZoneTableTable>();
	if (zoneTable != nullptr) {
		const CDZoneTable* zone = zoneTable->Query(zoneID.GetMapID());

		if (zone != nullptr) {
			zoneControlTemplate = zone->zoneControlTemplate != -1 ? zone->zoneControlTemplate : 2365;
			const auto min = zone->ghostdistance_min != -1.0f ? zone->ghostdistance_min : 100;
			const auto max = zone->ghostdistance != -1.0f ? zone->ghostdistance : 100;
			Game::entityManager->SetGhostDistanceMax(max + min);
			Game::entityManager->SetGhostDistanceMin(max);
			m_PlayerLoseCoinsOnDeath = zone->PlayerLoseCoinsOnDeath;
		}
	}

	LOG("Creating zone control object %i", zoneControlTemplate);

	// Create ZoneControl object
	if (!Game::entityManager) {
		LOG("ERROR: No entity manager loaded. Cannot proceed.");
		throw std::invalid_argument("No entity manager loaded. Cannot proceed.");
	}
	Game::entityManager->Initialize();
	EntityInfo info;
	info.lot = zoneControlTemplate;
	info.id = 70368744177662;
	Entity* zoneControl = Game::entityManager->CreateEntity(info, nullptr, nullptr, true);
	m_ZoneControlObject = zoneControl;

	m_pZone->Initalize();

	endTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

	LoadWorldConfig();

	LOG("Zone prepared in: %llu ms", (endTime - startTime));

	VanityUtilities::SpawnVanity();
}

dZoneManager::~dZoneManager() {
	if (m_pZone) delete m_pZone;

	for (std::pair<LWOOBJID, Spawner*> p : m_Spawners) {
		if (p.second) {
			delete p.second;
			p.second = nullptr;
		}

		m_Spawners.erase(p.first);
	}
	if (m_WorldConfig) delete m_WorldConfig;
}

Zone* dZoneManager::GetZone() {
	return m_pZone;
}

void dZoneManager::LoadZone(const LWOZONEID& zoneID) {
	if (m_pZone) delete m_pZone;

	m_ZoneID = zoneID;
	m_pZone = new Zone(zoneID.GetMapID(), zoneID.GetInstanceID(), zoneID.GetCloneID());
}

void dZoneManager::NotifyZone(const dZoneNotifier& notifier, const LWOOBJID& objectID) {
	switch (notifier) {
	case dZoneNotifier::SpawnedObjectDestroyed:
		break;
	case dZoneNotifier::SpawnedChildObjectDestroyed:
		break;
	case dZoneNotifier::ReloadZone:
		LOG("Forcing reload of zone %i", m_ZoneID.GetMapID());
		LoadZone(m_ZoneID);

		m_pZone->Initalize();
		break;
	case dZoneNotifier::UserJoined:
		break;
	case dZoneNotifier::UserMoved:
		break;
	case dZoneNotifier::PrintAllGameObjects:
		m_pZone->PrintAllGameObjects();
		break;
	case dZoneNotifier::InvalidNotifier:
		LOG("Got an invalid zone notifier.");
		break;
	default:
		LOG("Unknown zone notifier: %i", int(notifier));
	}
}

void dZoneManager::AddSpawner(LWOOBJID id, Spawner* spawner) {
	m_Spawners.insert_or_assign(id, spawner);
}

LWOZONEID dZoneManager::GetZoneID() const {
	return m_ZoneID;
}

void dZoneManager::Update(float deltaTime) {
	for (auto spawner : m_Spawners) {
		spawner.second->Update(deltaTime);
	}
}

LWOOBJID dZoneManager::MakeSpawner(SpawnerInfo info) {
	auto objectId = info.spawnerID;

	if (objectId == LWOOBJID_EMPTY) {
		objectId = ObjectIDManager::Instance()->GenerateObjectID();
		GeneralUtils::SetBit(objectId, eObjectBits::CLIENT);

		info.spawnerID = objectId;
	}

	auto* spawner = new Spawner(info);

	EntityInfo entityInfo{};

	entityInfo.id = objectId;
	entityInfo.lot = 176;

	auto* entity = Game::entityManager->CreateEntity(entityInfo, nullptr, nullptr, false, objectId);

	Game::entityManager->ConstructEntity(entity);

	AddSpawner(objectId, spawner);

	return objectId;
}

Spawner* dZoneManager::GetSpawner(const LWOOBJID id) {
	const auto& index = m_Spawners.find(id);

	if (index == m_Spawners.end()) {
		return nullptr;
	}

	return index->second;
}

void dZoneManager::RemoveSpawner(const LWOOBJID id) {
	auto* spawner = GetSpawner(id);

	if (spawner == nullptr) {
		LOG("Failed to find spawner (%llu)", id);
		return;
	}

	auto* entity = Game::entityManager->GetEntity(id);

	if (entity != nullptr) {
		entity->Kill();
	} else {

		LOG("Failed to find spawner entity (%llu)", id);
	}

	spawner->DestroyAllEntities();

	spawner->Deactivate();

	LOG("Destroying spawner (%llu)", id);

	m_Spawners.erase(id);

	delete spawner;
}


std::vector<Spawner*> dZoneManager::GetSpawnersByName(std::string spawnerName) {
	std::vector<Spawner*> spawners;
	for (const auto& spawner : m_Spawners) {
		if (spawner.second->GetName() == spawnerName) {
			spawners.push_back(spawner.second);
		}
	}

	return spawners;
}

std::vector<Spawner*> dZoneManager::GetSpawnersInGroup(std::string group) {
	std::vector<Spawner*> spawnersInGroup;
	for (auto spawner : m_Spawners) {
		for (std::string entityGroup : spawner.second->m_Info.groups) {
			if (entityGroup == group) {
				spawnersInGroup.push_back(spawner.second);
			}
		}
	}

	return spawnersInGroup;
}

uint32_t dZoneManager::GetUniqueMissionIdStartingValue() {
	if (m_UniqueMissionIdStart == 0) {
		auto tableData = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM Missions WHERE isMission = 0 GROUP BY isMission;");
		m_UniqueMissionIdStart = tableData.getIntField(0, -1);
		tableData.finalize();
	}
	return m_UniqueMissionIdStart;
}

bool dZoneManager::CheckIfAccessibleZone(LWOMAPID zoneID) {
	//We're gonna go ahead and presume we've got the db loaded already:
	CDZoneTableTable* zoneTable = CDClientManager::Instance().GetTable<CDZoneTableTable>();
	const CDZoneTable* zone = zoneTable->Query(zoneID);
	if (zone != nullptr) {
		return Game::assetManager->HasFile(("maps/" + zone->zoneName).c_str());
	} else {
		return false;
	}
}

void dZoneManager::LoadWorldConfig() {
	LOG("Loading WorldConfig into memory");

	auto worldConfig = CDClientDatabase::ExecuteQuery("SELECT * FROM WorldConfig;");

	if (!m_WorldConfig) m_WorldConfig = new WorldConfig();

	if (worldConfig.eof()) {
		LOG("WorldConfig table is empty.  Is this intended?");
		return;
	}

	// Now read in the giant table
	m_WorldConfig->worldConfigID = worldConfig.getIntField("WorldConfigID");
	m_WorldConfig->peGravityValue = worldConfig.getFloatField("pegravityvalue");
	m_WorldConfig->peBroadphaseWorldSize = worldConfig.getFloatField("pebroadphaseworldsize");
	m_WorldConfig->peGameObjScaleFactor = worldConfig.getFloatField("pegameobjscalefactor");
	m_WorldConfig->characterRotationSpeed = worldConfig.getFloatField("character_rotation_speed");
	m_WorldConfig->characterWalkForwardSpeed = worldConfig.getFloatField("character_walk_forward_speed");
	m_WorldConfig->characterWalkBackwardSpeed = worldConfig.getFloatField("character_walk_backward_speed");
	m_WorldConfig->characterWalkStrafeSpeed = worldConfig.getFloatField("character_walk_strafe_speed");
	m_WorldConfig->characterWalkStrafeForwardSpeed = worldConfig.getFloatField("character_walk_strafe_forward_speed");
	m_WorldConfig->characterWalkStrafeBackwardSpeed = worldConfig.getFloatField("character_walk_strafe_backward_speed");
	m_WorldConfig->characterRunBackwardSpeed = worldConfig.getFloatField("character_run_backward_speed");
	m_WorldConfig->characterRunStrafeSpeed = worldConfig.getFloatField("character_run_strafe_speed");
	m_WorldConfig->characterRunStrafeForwardSpeed = worldConfig.getFloatField("character_run_strafe_forward_speed");
	m_WorldConfig->characterRunStrafeBackwardSpeed = worldConfig.getFloatField("character_run_strafe_backward_speed");
	m_WorldConfig->globalCooldown = worldConfig.getFloatField("global_cooldown");
	m_WorldConfig->characterGroundedTime = worldConfig.getFloatField("characterGroundedTime");
	m_WorldConfig->characterGroundedSpeed = worldConfig.getFloatField("characterGroundedSpeed");
	m_WorldConfig->globalImmunityTime = worldConfig.getFloatField("globalImmunityTime");
	m_WorldConfig->characterMaxSlope = worldConfig.getFloatField("character_max_slope");
	m_WorldConfig->defaultRespawnTime = worldConfig.getFloatField("defaultrespawntime");
	m_WorldConfig->missionTooltipTimeout = worldConfig.getFloatField("mission_tooltip_timeout");
	m_WorldConfig->vendorBuyMultiplier = worldConfig.getFloatField("vendor_buy_multiplier", 0.1);
	m_WorldConfig->petFollowRadius = worldConfig.getFloatField("pet_follow_radius");
	m_WorldConfig->characterEyeHeight = worldConfig.getFloatField("character_eye_height");
	m_WorldConfig->flightVerticalVelocity = worldConfig.getFloatField("flight_vertical_velocity");
	m_WorldConfig->flightAirspeed = worldConfig.getFloatField("flight_airspeed");
	m_WorldConfig->flightFuelRatio = worldConfig.getFloatField("flight_fuel_ratio");
	m_WorldConfig->flightMaxAirspeed = worldConfig.getFloatField("flight_max_airspeed");
	m_WorldConfig->fReputationPerVote = worldConfig.getFloatField("fReputationPerVote");
	m_WorldConfig->propertyCloneLimit = worldConfig.getIntField("nPropertyCloneLimit");
	m_WorldConfig->defaultHomespaceTemplate = worldConfig.getIntField("defaultHomespaceTemplate");
	m_WorldConfig->coinsLostOnDeathPercent = worldConfig.getFloatField("coins_lost_on_death_percent");
	m_WorldConfig->coinsLostOnDeathMin = worldConfig.getIntField("coins_lost_on_death_min");
	m_WorldConfig->coinsLostOnDeathMax = worldConfig.getIntField("coins_lost_on_death_max");
	m_WorldConfig->characterVotesPerDay = worldConfig.getIntField("character_votes_per_day");
	m_WorldConfig->propertyModerationRequestApprovalCost = worldConfig.getIntField("property_moderation_request_approval_cost");
	m_WorldConfig->propertyModerationRequestReviewCost = worldConfig.getIntField("property_moderation_request_review_cost");
	m_WorldConfig->propertyModRequestsAllowedSpike = worldConfig.getIntField("propertyModRequestsAllowedSpike");
	m_WorldConfig->propertyModRequestsAllowedInterval = worldConfig.getIntField("propertyModRequestsAllowedInterval");
	m_WorldConfig->propertyModRequestsAllowedTotal = worldConfig.getIntField("propertyModRequestsAllowedTotal");
	m_WorldConfig->propertyModRequestsSpikeDuration = worldConfig.getIntField("propertyModRequestsSpikeDuration");
	m_WorldConfig->propertyModRequestsIntervalDuration = worldConfig.getIntField("propertyModRequestsIntervalDuration");
	m_WorldConfig->modelModerateOnCreate = worldConfig.getIntField("modelModerateOnCreate") != 0;
	m_WorldConfig->defaultPropertyMaxHeight = worldConfig.getFloatField("defaultPropertyMaxHeight");
	m_WorldConfig->reputationPerVoteCast = worldConfig.getFloatField("reputationPerVoteCast");
	m_WorldConfig->reputationPerVoteReceived = worldConfig.getFloatField("reputationPerVoteReceived");
	m_WorldConfig->showcaseTopModelConsiderationBattles = worldConfig.getIntField("showcaseTopModelConsiderationBattles");
	m_WorldConfig->reputationPerBattlePromotion = worldConfig.getFloatField("reputationPerBattlePromotion");
	m_WorldConfig->coinsLostOnDeathMinTimeout = worldConfig.getFloatField("coins_lost_on_death_min_timeout");
	m_WorldConfig->coinsLostOnDeathMaxTimeout = worldConfig.getFloatField("coins_lost_on_death_max_timeout");
	m_WorldConfig->mailBaseFee = worldConfig.getIntField("mail_base_fee");
	m_WorldConfig->mailPercentAttachmentFee = worldConfig.getFloatField("mail_percent_attachment_fee");
	m_WorldConfig->propertyReputationDelay = worldConfig.getIntField("propertyReputationDelay");
	m_WorldConfig->levelCap = worldConfig.getIntField("LevelCap");
	m_WorldConfig->levelUpBehaviorEffect = worldConfig.getStringField("LevelUpBehaviorEffect");
	m_WorldConfig->characterVersion = worldConfig.getIntField("CharacterVersion");
	m_WorldConfig->levelCapCurrencyConversion = worldConfig.getIntField("LevelCapCurrencyConversion");
	worldConfig.finalize();
	LOG("Loaded WorldConfig into memory");
}
