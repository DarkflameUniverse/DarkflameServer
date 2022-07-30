#include "dZoneManager.h"
#include "PetDigServer.h"
#include "MissionComponent.h"
#include "EntityManager.h"
#include "Character.h"
#include "PetComponent.h"

std::vector<LWOOBJID> PetDigServer::treasures{};

const DigInfo PetDigServer::defaultDigInfo = DigInfo{ 3495, -1, -1, false, false, false, false };

/**
 * Summary of all the special treasure behaviors, indexed by their lot
 */
const std::map<LOT, DigInfo> PetDigServer::digInfoMap{
	// Regular treasures
	{3495, defaultDigInfo},

	// Pet cove treasure
	{7612, DigInfo { 7612, -1, -1, false, false, false, false }},

	// Gnarled Forest flag treasure
	{7410, DigInfo { 7410, -1, -1, false, true, false, false }},

	// Gnarled Forest crab treasure
	{9308, DigInfo { 9308, 7694, -1, false, false, false, false }},

	// Avant Gardens mission treasure
	{9307, DigInfo { 9307, -1, -1, false, true, false, true }},

	// Avant Gardens bouncer treasure
	{7559, DigInfo { 7559, -1, -1, false, false, true, false }},

	// Crux Prime dragon treasure
	{13098, DigInfo { 13098, 13067, 1298, false, false, false, false }},

	// Bone treasure (can only be digged using the dragon)
	{12192, DigInfo { 12192, -1, -1, true, false, false, false }},
};

void PetDigServer::OnStartup(Entity* self) {
	treasures.push_back(self->GetObjectID());
	const auto digInfoIterator = digInfoMap.find(self->GetLOT());
	const auto digInfo = digInfoIterator != digInfoMap.end() ? digInfoIterator->second : defaultDigInfo;

	// Reset any bouncers that might've been created by the previous dig
	if (digInfo.bouncer) {
		auto bounceNumber = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"BouncerNumber"));
		auto bouncerSpawners = dZoneManager::Instance()->GetSpawnersByName("PetBouncer" + bounceNumber);
		auto switchSpawners = dZoneManager::Instance()->GetSpawnersByName("PetBouncerSwitch" + bounceNumber);

		for (auto* bouncerSpawner : bouncerSpawners) {
			for (auto* bouncer : bouncerSpawner->m_Info.nodes)
				bouncerSpawner->Deactivate();
			bouncerSpawner->Reset();
		}

		for (auto* switchSpawner : switchSpawners) {
			switchSpawner->Deactivate();
			switchSpawner->Reset();
		}
	}
}

void PetDigServer::OnDie(Entity* self, Entity* killer) {
	const auto iterator = std::find(treasures.begin(), treasures.end(), self->GetObjectID());
	if (iterator != treasures.end()) {
		treasures.erase(iterator);
	}

	auto* owner = killer->GetOwner();
	const auto digInfoIterator = digInfoMap.find(self->GetLOT());
	const auto digInfo = digInfoIterator != digInfoMap.end() ? digInfoIterator->second : defaultDigInfo;

	if (digInfo.spawnLot >= 0) {
		PetDigServer::SpawnPet(self, owner, digInfo);
	} else if (digInfo.builderOnly) {

		// Some treasures may only be retrieved by the player that built the diggable
		auto builder = self->GetVar<LWOOBJID>(u"builder"); // Set by the pet dig build script
		if (builder != owner->GetObjectID())
			return;
	} else if (digInfo.xBuild) {
		PetDigServer::HandleXBuildDig(self, owner, killer);
		return;
	} else if (digInfo.bouncer) {
		PetDigServer::HandleBouncerDig(self, owner);
	}

	PetDigServer::ProgressPetDigMissions(owner, self);

	self->SetNetworkVar<bool>(u"treasure_dug", true);
	// TODO: Reset other pets

	// Handles smashing leftovers (edge case for the AG X)
	auto* xObject = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"X"));
	if (xObject != nullptr) {
		xObject->Smash(xObject->GetObjectID(), VIOLENT);
	}
}

void PetDigServer::HandleXBuildDig(const Entity* self, Entity* owner, Entity* pet) {
	auto playerID = self->GetVar<LWOOBJID>(u"builder");
	if (playerID == LWOOBJID_EMPTY || playerID != owner->GetObjectID())
		return;

	auto* playerEntity = EntityManager::Instance()->GetEntity(playerID);
	if (!playerEntity || !playerEntity->GetParentUser() || !playerEntity->GetParentUser()->GetLastUsedChar())
		return;

	auto* player = playerEntity->GetCharacter();
	const auto groupID = self->GetVar<std::u16string>(u"groupID");
	auto playerFlag = 0;

	// The flag that the player dug up
	if (groupID == u"Flag1") {
		playerFlag = 61;
	} else if (groupID == u"Flag2") {
		playerFlag = 62;
	} else if (groupID == u"Flag3") {
		playerFlag = 63;
	}

	// If the player doesn't have the flag yet
	if (playerFlag != 0 && !player->GetPlayerFlag(playerFlag)) {
		auto* petComponent = pet->GetComponent<PetComponent>();
		if (petComponent != nullptr) {
			// TODO: Pet state = 9 ??
		}

		// Shows the flag object to the player
		player->SetPlayerFlag(playerFlag, true);
	}

	auto* xObject = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"X"));
	if (xObject != nullptr) {
		xObject->Smash(xObject->GetObjectID(), VIOLENT);
	}
}

void PetDigServer::HandleBouncerDig(const Entity* self, const Entity* owner) {
	auto bounceNumber = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"BouncerNumber"));
	auto bouncerSpawners = dZoneManager::Instance()->GetSpawnersByName("PetBouncer" + bounceNumber);
	auto switchSpawners = dZoneManager::Instance()->GetSpawnersByName("PetBouncerSwitch" + bounceNumber);

	for (auto* bouncerSpawner : bouncerSpawners) {
		bouncerSpawner->Activate();
	}

	for (auto* switchSpawner : switchSpawners) {
		switchSpawner->Activate();
	}
}

/**
 * Progresses the Can You Dig It mission and the Pet Excavator Achievement if the player has never completed it yet
 * \param owner the owner that just made a pet dig something up
 */
void PetDigServer::ProgressPetDigMissions(const Entity* owner, const Entity* chest) {
	auto* missionComponent = owner->GetComponent<MissionComponent>();

	if (missionComponent != nullptr) {
		// Can You Dig It progress
		const auto digMissionState = missionComponent->GetMissionState(843);
		if (digMissionState == MissionState::MISSION_STATE_ACTIVE) {
			missionComponent->ForceProgress(843, 1216, 1);
		}

		// Pet Excavator progress
		const auto excavatorMissionState = missionComponent->GetMissionState(505);
		if (excavatorMissionState == MissionState::MISSION_STATE_ACTIVE) {
			if (chest->HasVar(u"PetDig")) {
				int32_t playerFlag = 1260 + chest->GetVarAs<int32_t>(u"PetDig");
				Character* player = owner->GetCharacter();

				// check if player flag is set
				if (!player->GetPlayerFlag(playerFlag)) {
					missionComponent->ForceProgress(505, 767, 1);
					player->SetPlayerFlag(playerFlag, 1);
				}
			}
		}
	}
}

/**
 * Some treasures spawn special pets, this handles that case
 * \param owner the owner that just made a pet dig something up
 * \param digInfo information regarding the treasure, will also contain info about the pet to spawn
 */
void PetDigServer::SpawnPet(Entity* self, const Entity* owner, const DigInfo digInfo) {
	// Some treasures require a mission to be active
	if (digInfo.requiredMission >= 0) {
		auto* missionComponent = owner->GetComponent<MissionComponent>();
		if (missionComponent != nullptr && missionComponent->GetMissionState(digInfo.requiredMission) < MissionState::MISSION_STATE_ACTIVE) {
			return;
		}
	}

	EntityInfo info{};
	info.lot = digInfo.spawnLot;
	info.pos = self->GetPosition();
	info.rot = self->GetRotation();
	info.spawnerID = self->GetSpawnerID();
	info.settings = {
			new LDFData<LWOOBJID>(u"tamer", owner->GetObjectID()),
			new LDFData<std::string>(u"group", "pet" + std::to_string(owner->GetObjectID())),
			new LDFData<std::string>(u"spawnAnim", "spawn-pet"),
			new LDFData<float>(u"spawnTimer", 1.0)
	};

	auto* spawnedPet = EntityManager::Instance()->CreateEntity(info);
	EntityManager::Instance()->ConstructEntity(spawnedPet);
}

Entity* PetDigServer::GetClosestTresure(NiPoint3 position) {
	float closestDistance = 0;
	Entity* closest = nullptr;

	for (const auto tresureId : treasures) {
		auto* tresure = EntityManager::Instance()->GetEntity(tresureId);

		if (tresure == nullptr) continue;

		float distance = Vector3::DistanceSquared(tresure->GetPosition(), position);

		if (closest == nullptr || distance < closestDistance) {
			closestDistance = distance;
			closest = tresure;
		}
	}

	return closest;
}
