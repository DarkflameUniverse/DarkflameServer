#include "PetComponent.h"
#include "GameMessages.h"
#include "BrickDatabase.h"
#include "CDClientDatabase.h"
#include "ChatPackets.h"
#include "EntityManager.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "MissionComponent.h"
#include "SwitchComponent.h"
#include "DestroyableComponent.h"
#include "dpWorld.h"
#include "PetDigServer.h"
#include "../dWorldServer/ObjectIDManager.h"
#include "eUnequippableActiveType.h"
#include "eTerminateType.h"
#include "ePetTamingNotifyType.h"
#include "eUseItemResponse.h"
#include "ePlayerFlag.h"
#include "MovementAIComponent.h"
#include "Preconditions.h"

#include "Game.h"
#include "dConfig.h"
#include "dChatFilter.h"
#include "Database.h"
#include "EntityInfo.h"
#include "eMissionTaskType.h"
#include "RenderComponent.h"
#include "eObjectBits.h"
#include "eGameMasterLevel.h"

std::unordered_map<LOT, PetComponent::PetPuzzleData> PetComponent::buildCache;
std::unordered_map<LWOOBJID, LWOOBJID> PetComponent::currentActivities;
std::unordered_map<LWOOBJID, LWOOBJID> PetComponent::activePets;

/**
 * Maps all the pet lots to a flag indicating that the player has caught it. All basic pets have been guessed by ObjID
 * while the faction ones could be checked using their respective missions.
 */
std::map<LOT, ePlayerFlag> PetComponent::petFlags = {
		{ 3050, ePlayerFlag::ELEPHANT_PET_3050 },
		{ 3054, ePlayerFlag::CAT_PET_3054 },
		{ 3195, ePlayerFlag::TRICERATOPS_PET_3195 },
		{ 3254, ePlayerFlag::TERRIER_PET_3254 },
		{ 3261, ePlayerFlag::SKUNK_PET_3261 },
		{ 3672, ePlayerFlag::BUNNY_PET_3672 },
		{ 3994, ePlayerFlag::CROCODILE_PET_3994 },
		{ 5635, ePlayerFlag::DOBERMAN_PET_5635 },
		{ 5636, ePlayerFlag::BUFFALO_PET_5636 },
		{ 5637, ePlayerFlag::ROBOT_DOG_PET_5637 },
		{ 5639, ePlayerFlag::RED_DRAGON_PET_5639 },
		{ 5640, ePlayerFlag::TORTOISE_PET_5640 },
		{ 5641, ePlayerFlag::GREEN_DRAGON_PET_5641 },
		{ 5643, ePlayerFlag::PANDA_PET_5643 },
		{ 5642, ePlayerFlag::MANTIS_PET_5642 },
		{ 6720, ePlayerFlag::WARTHOG_PET_6720 },
		{ 3520, ePlayerFlag::LION_PET_3520 },
		{ 7638, ePlayerFlag::GOAT_PET_7638 },
		{ 7694, ePlayerFlag::CRAB_PET_7694 },
		{ 12294, ePlayerFlag::REINDEER_PET_12294 },
		{ 12431, ePlayerFlag::STEGOSAURUS_PET_12431 },
		{ 12432, ePlayerFlag::SABER_CAT_PET_12432 },
		{ 12433, ePlayerFlag::GRYPHON_PET_12433 },
		{ 12434, ePlayerFlag::ALINE_PET_12334 },
		// 834: Bone dragon pet?, see mission 506, 688 
		{ 16210, ePlayerFlag::EARTH_DRAGON_PET_16210 },
		{ 13067, ePlayerFlag::SKELETON_DRAGON_PET_13067 },
};

PetComponent::PetComponent(Entity* parent, uint32_t componentId) : Component(parent) {
	m_ComponentId = componentId;

	m_Interaction = LWOOBJID_EMPTY;
	m_Owner = LWOOBJID_EMPTY;
	m_ModerationStatus = 0;
	m_Tamer = LWOOBJID_EMPTY;
	m_ModelId = LWOOBJID_EMPTY;
	m_Timer = 0;
	m_TimerAway = 0;
	m_DatabaseId = LWOOBJID_EMPTY;
	m_Status = 67108866; // Tamable
	m_Ability = PetAbilityType::Invalid;
	m_StartPosition = NiPoint3::ZERO;
	m_MovementAI = nullptr;
	m_TresureTime = 0;
	m_Preconditions = nullptr;
	m_ImaginationDrainRate = 60.0f;

	auto checkPreconditions = GeneralUtils::UTF16ToWTF8(parent->GetVar<std::u16string>(u"CheckPrecondition"));

	if (!checkPreconditions.empty()) {
		SetPreconditions(checkPreconditions);
	}
	// Get the imagination drain rate from the CDClient
	auto query = CDClientDatabase::CreatePreppedStmt("SELECT m_ImaginationDrainRate FROM PetComponent WHERE id = ?;");

	query.bind(1, static_cast<int>(componentId));

	auto result = query.execQuery();

	// Should a result not exist for this pet default to 60 seconds.
	if (result.eof()) return;

	m_ImaginationDrainRate = result.getFloatField(0, 60.0f);
}

void PetComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	const bool tamed = m_Owner != LWOOBJID_EMPTY;

	outBitStream->Write1(); // Always serialize as dirty for now

	outBitStream->Write<uint32_t>(m_Status);
	outBitStream->Write(tamed ? m_Ability : PetAbilityType::Invalid); // Something with the overhead icon?

	const bool interacting = m_Interaction != LWOOBJID_EMPTY;

	outBitStream->Write(interacting);
	if (interacting) outBitStream->Write(m_Interaction);

	outBitStream->Write(tamed);
	if (tamed) outBitStream->Write(m_Owner);

	if (!bIsInitialUpdate) return;

	outBitStream->Write(tamed);
	if (tamed) {
		outBitStream->Write(m_ModerationStatus);

		outBitStream->Write<uint8_t>(m_Name.size());
		for (const auto c : m_Name) {
			outBitStream->Write<char16_t>(c);
		}

		outBitStream->Write<uint8_t>(m_OwnerName.size());
		for (const auto c : m_OwnerName) {
			outBitStream->Write<char16_t>(c);
		}
	}
}

void PetComponent::OnUse(Entity* originator) {
	if (m_Owner != LWOOBJID_EMPTY) return;

	if (m_Tamer != LWOOBJID_EMPTY) {
		auto* tamer = EntityManager::Instance()->GetEntity(m_Tamer);

		if (tamer) return;

		m_Tamer = LWOOBJID_EMPTY;
	}

	auto* inventoryComponent = originator->GetComponent<InventoryComponent>();

	if (!inventoryComponent) return;

	if (m_Preconditions && !m_Preconditions->Check(originator, true)) {
		return;
	}

	auto* movementAIComponent = m_ParentEntity->GetComponent<MovementAIComponent>();

	if (movementAIComponent) {
		movementAIComponent->Stop();
	}

	inventoryComponent->DespawnPet();

	const auto& cached = buildCache.find(m_ParentEntity->GetLOT());
	int32_t imaginationCost = 0;

	std::string buildFile;

	if (cached == buildCache.end()) {
		auto query = CDClientDatabase::CreatePreppedStmt(
			"SELECT ValidPiecesLXF, PuzzleModelLot, Timelimit, NumValidPieces, imagCostPerBuild FROM TamingBuildPuzzles WHERE NPCLot = ?;");
		query.bind(1, static_cast<int32_t>(m_ParentEntity->GetLOT()));

		auto result = query.execQuery();

		if (result.eof()) {
			ChatPackets::SendSystemMessage(originator->GetSystemAddress(), u"Failed to find the puzzle minigame for this pet.");

			return;
		}

		if (result.fieldIsNull(0)) return;

		buildFile = std::string(result.getStringField(0));

		PetPuzzleData data;
		data.buildFile = buildFile;
		data.puzzleModelLot = result.getIntField(1);
		data.timeLimit = result.getFloatField(2);
		data.numValidPieces = result.getIntField(3);
		data.imaginationCost = result.getIntField(4);
		if (data.timeLimit <= 0) data.timeLimit = 60;
		imaginationCost = data.imaginationCost;

		buildCache[m_ParentEntity->GetLOT()] = data;
	} else {
		buildFile = cached->second.buildFile;
		imaginationCost = cached->second.imaginationCost;
	}

	auto* destroyableComponent = originator->GetComponent<DestroyableComponent>();

	if (!destroyableComponent) return;

	auto imagination = destroyableComponent->GetImagination();

	if (imagination < imaginationCost) return;

	auto& bricks = BrickDatabase::Instance()->GetBricks(buildFile);

	if (bricks.empty()) {
		ChatPackets::SendSystemMessage(originator->GetSystemAddress(), u"Failed to load the puzzle minigame for this pet.");
		Game::logger->Log("PetComponent", "Couldn't find %s for minigame!", buildFile.c_str());

		return;
	}

	auto petPosition = m_ParentEntity->GetPosition();

	auto originatorPosition = originator->GetPosition();

	m_ParentEntity->SetRotation(NiQuaternion::LookAt(petPosition, originatorPosition));

	float interactionDistance = m_ParentEntity->GetVar<float>(u"interaction_distance");

	if (interactionDistance <= 0) {
		interactionDistance = 15;
	}

	auto position = originatorPosition;

	NiPoint3 forward = NiQuaternion::LookAt(m_ParentEntity->GetPosition(), originator->GetPosition()).GetForwardVector();
	forward.y = 0.0f;

	if (dpWorld::Instance().IsLoaded()) {
		NiPoint3 attempt = petPosition + forward * interactionDistance;

		float y = dpWorld::Instance().GetNavMesh()->GetHeightAtPoint(attempt);

		while (std::abs(y - petPosition.y) > 4 && interactionDistance > 10) {
			const NiPoint3 forward = m_ParentEntity->GetRotation().GetForwardVector();

			attempt = originatorPosition + forward * interactionDistance;

			y = dpWorld::Instance().GetNavMesh()->GetHeightAtPoint(attempt);

			interactionDistance -= 0.5f;
		}

		position = attempt;
	} else {
		position = petPosition + forward * interactionDistance;
	}


	auto rotation = NiQuaternion::LookAt(position, petPosition);

	GameMessages::SendNotifyPetTamingMinigame(
		originator->GetObjectID(),
		m_ParentEntity->GetObjectID(),
		LWOOBJID_EMPTY,
		true,
		ePetTamingNotifyType::BEGIN,
		petPosition,
		position,
		rotation,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	GameMessages::SendNotifyPetTamingMinigame(
		m_ParentEntity->GetObjectID(),
		LWOOBJID_EMPTY,
		originator->GetObjectID(),
		true,
		ePetTamingNotifyType::BEGIN,
		petPosition,
		position,
		rotation,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	GameMessages::SendNotifyPetTamingPuzzleSelected(originator->GetObjectID(), bricks, originator->GetSystemAddress());

	m_Tamer = originator->GetObjectID();
	SetStatus(5);

	currentActivities.insert_or_assign(m_Tamer, m_ParentEntity->GetObjectID());

	// Notify the start of a pet taming minigame
	m_ParentEntity->GetScript()->OnNotifyPetTamingMinigame(m_ParentEntity, originator, ePetTamingNotifyType::BEGIN);

}

void PetComponent::Update(float deltaTime) {
	if (m_StartPosition == NiPoint3::ZERO) {
		m_StartPosition = m_ParentEntity->GetPosition();
	}

	if (m_Owner == LWOOBJID_EMPTY) {
		if (m_Tamer != LWOOBJID_EMPTY) {
			if (m_Timer > 0) {
				m_Timer -= deltaTime;

				if (m_Timer <= 0) {
					m_Timer = 0;

					ClientFailTamingMinigame();
				}
			}
		} else {
			if (m_Timer > 0) {
				m_Timer -= deltaTime;

				if (m_Timer <= 0) {
					Wander();
					EntityManager::Instance()->SerializeEntity(m_ParentEntity);
				}
			} else {
				m_Timer = 5;
			}
		}

		return;
	}

	auto* owner = GetOwner();

	if (!owner) {
		m_ParentEntity->Kill();

		return;
	}

	if (!m_MovementAI) {
		m_MovementAI = m_ParentEntity->GetComponent<MovementAIComponent>();
		if (!m_MovementAI) return;
	}

	if (m_TresureTime > 0) {
		auto* tresure = EntityManager::Instance()->GetEntity(m_Interaction);

		if (!tresure) {
			m_TresureTime = 0;

			return;
		}

		m_TresureTime -= deltaTime;

		m_MovementAI->Stop();

		if (m_TresureTime <= 0) {
			m_ParentEntity->SetOwnerOverride(m_Owner);

			tresure->Smash(m_ParentEntity->GetObjectID());

			m_Interaction = LWOOBJID_EMPTY;

			m_TresureTime = 0;
		}

		return;
	}

	auto destination = owner->GetPosition();
	NiPoint3 position = m_MovementAI->GetCurrentPosition();

	float distanceToOwner = Vector3::DistanceSquared(position, destination);

	if (distanceToOwner > 50 * 50 || m_TimerAway > 5) {
		m_MovementAI->Warp(destination);

		m_Timer = 1;
		m_TimerAway = 0;

		return;
	}

	if (distanceToOwner > 15 * 15 || std::abs(destination.y - position.y) >= 3) {
		m_TimerAway += deltaTime;
	} else {
		m_TimerAway = 0;
	}

	if (m_Timer > 0) {
		m_Timer -= deltaTime;

		return;
	}

	auto* closestSwitch = SwitchComponent::GetClosestSwitch(position);

	float haltDistance = 5;

	if (closestSwitch) {
		if (!closestSwitch->GetActive()) {
			NiPoint3 switchPosition = closestSwitch->GetParentEntity()->GetPosition();
			float distance = Vector3::DistanceSquared(position, switchPosition);
			if (distance < 3 * 3) {
				m_Interaction = closestSwitch->GetParentEntity()->GetObjectID();
				closestSwitch->EntityEnter(m_ParentEntity);
			} else if (distance < 20 * 20) {
				haltDistance = 1;

				destination = switchPosition;
			}
		}
	}

	Entity* closestTresure = PetDigServer::GetClosestTresure(position);

	if (closestTresure) {
		// Skeleton Dragon Pat special case for bone digging
		if (closestTresure->GetLOT() == 12192 && m_ParentEntity->GetLOT() != 13067) {
			goto skipTresure;
		}

		NiPoint3 tresurePosition = closestTresure->GetPosition();
		float distance = Vector3::DistanceSquared(position, tresurePosition);
		if (distance < 3 * 3) {
			m_Interaction = closestTresure->GetObjectID();

			Command(NiPoint3::ZERO, LWOOBJID_EMPTY, 1, 202, true);

			m_TresureTime = 2;
		} else if (distance < 10 * 10) {
			haltDistance = 1;

			destination = tresurePosition;
		}
	}

skipTresure:

	m_MovementAI->SetHaltDistance(haltDistance);

	m_MovementAI->SetSpeed(2.5f);

	m_MovementAI->SetDestination(destination);

	m_Timer = 1.0f;
}

void PetComponent::TryBuild(uint32_t numBricks, bool clientFailed) {
	if (m_Tamer == LWOOBJID_EMPTY) return;

	auto* tamer = EntityManager::Instance()->GetEntity(m_Tamer);

	if (!tamer) {
		m_Tamer = LWOOBJID_EMPTY;

		return;
	}

	const auto& cached = buildCache.find(m_ParentEntity->GetLOT());

	if (cached == buildCache.end()) return;

	auto* destroyableComponent = tamer->GetComponent<DestroyableComponent>();

	if (!destroyableComponent) return;

	auto imagination = destroyableComponent->GetImagination();

	imagination -= cached->second.imaginationCost;

	destroyableComponent->SetImagination(imagination);

	EntityManager::Instance()->SerializeEntity(tamer);

	if (clientFailed) {
		if (imagination < cached->second.imaginationCost) {
			ClientFailTamingMinigame();
		}
	} else {
		m_Timer = 0;
	}

	if (numBricks == 0) return;

	GameMessages::SendPetTamingTryBuildResult(m_Tamer, !clientFailed, numBricks, tamer->GetSystemAddress());
}

void PetComponent::NotifyTamingBuildSuccess(NiPoint3 position) {
	if (m_Tamer == LWOOBJID_EMPTY) return;

	auto* tamer = EntityManager::Instance()->GetEntity(m_Tamer);

	if (!tamer) {
		m_Tamer = LWOOBJID_EMPTY;

		return;
	}

	const auto& cached = buildCache.find(m_ParentEntity->GetLOT());

	if (cached == buildCache.end()) {
		return;
	}

	GameMessages::SendPlayFXEffect(tamer, -1, u"petceleb", "", LWOOBJID_EMPTY, 1, 1, true);
	RenderComponent::PlayAnimation(tamer, u"rebuild-celebrate");

	EntityInfo info{};
	info.lot = cached->second.puzzleModelLot;
	info.pos = position;
	info.rot = NiQuaternion::IDENTITY;
	info.spawnerID = tamer->GetObjectID();

	auto* modelEntity = EntityManager::Instance()->CreateEntity(info);

	m_ModelId = modelEntity->GetObjectID();

	EntityManager::Instance()->ConstructEntity(modelEntity);

	GameMessages::SendNotifyTamingModelLoadedOnServer(m_Tamer, tamer->GetSystemAddress());

	GameMessages::SendPetResponse(m_Tamer, m_ParentEntity->GetObjectID(), 0, 10, 0, tamer->GetSystemAddress());

	auto* inventoryComponent = tamer->GetComponent<InventoryComponent>();

	if (!inventoryComponent) return;

	LWOOBJID petSubKey = ObjectIDManager::Instance()->GenerateRandomObjectID();

	GeneralUtils::SetBit(petSubKey, eObjectBits::CHARACTER);
	GeneralUtils::SetBit(petSubKey, eObjectBits::PERSISTENT);

	m_DatabaseId = petSubKey;

	std::string petName = tamer->GetCharacter()->GetName();
	petName += "'s Pet";

	GameMessages::SendAddPetToPlayer(m_Tamer, 0, GeneralUtils::UTF8ToUTF16(petName), petSubKey, m_ParentEntity->GetLOT(), tamer->GetSystemAddress());

	GameMessages::SendRegisterPetID(m_Tamer, m_ParentEntity->GetObjectID(), tamer->GetSystemAddress());

	GameMessages::SendRegisterPetDBID(m_Tamer, petSubKey, tamer->GetSystemAddress());

	inventoryComponent->AddItem(m_ParentEntity->GetLOT(), 1, eLootSourceType::ACTIVITY, eInventoryType::MODELS, {}, LWOOBJID_EMPTY, true, false, petSubKey);
	auto* item = inventoryComponent->FindItemBySubKey(petSubKey, eInventoryType::MODELS);

	if (!item) return;

	DatabasePet databasePet{};

	databasePet.lot = m_ParentEntity->GetLOT();
	databasePet.moderationState = 1;
	databasePet.name = petName;

	inventoryComponent->SetDatabasePet(petSubKey, databasePet);

	Activate(item, false, true);

	m_Timer = 0;

	GameMessages::SendNotifyPetTamingMinigame(
		m_Tamer,
		LWOOBJID_EMPTY,
		LWOOBJID_EMPTY,
		false,
		ePetTamingNotifyType::NAMINGPET,
		NiPoint3::ZERO,
		NiPoint3::ZERO,
		NiQuaternion::IDENTITY,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	auto petFlag = petFlags.find(m_ParentEntity->GetLOT());
	// Triggers the catch a pet missions
	if (petFlag != petFlags.end()) {
		tamer->GetCharacter()->SetPlayerFlag(petFlag->second, true);
	}

	auto* missionComponent = tamer->GetComponent<MissionComponent>();

	if (missionComponent) {
		missionComponent->Progress(eMissionTaskType::PET_TAMING, m_ParentEntity->GetLOT());
	}

	SetStatus(1);

	auto* characterComponent = tamer->GetComponent<CharacterComponent>();
	if (characterComponent) {
		characterComponent->UpdatePlayerStatistic(PetsTamed);
	}
}

void PetComponent::RequestSetPetName(std::u16string name) {
	if (m_Tamer == LWOOBJID_EMPTY) {
		if (m_Owner != LWOOBJID_EMPTY) {
			auto* owner = GetOwner();

			m_ModerationStatus = 1; // Pending
			m_Name = "";

			//Save our pet's new name to the db:
			SetPetNameForModeration(GeneralUtils::UTF16ToWTF8(name));

			GameMessages::SendSetPetName(m_Owner, GeneralUtils::UTF8ToUTF16(m_Name), m_DatabaseId, owner->GetSystemAddress());
			GameMessages::SendSetPetNameModerated(m_Owner, m_DatabaseId, m_ModerationStatus, owner->GetSystemAddress());
		}

		return;
	}

	auto* tamer = EntityManager::Instance()->GetEntity(m_Tamer);

	if (!tamer) {
		m_Tamer = LWOOBJID_EMPTY;

		return;
	}

	Game::logger->Log("PetComponent", "Got set pet name (%s)", GeneralUtils::UTF16ToWTF8(name).c_str());

	auto* inventoryComponent = tamer->GetComponent<InventoryComponent>();

	if (!inventoryComponent) return;

	m_ModerationStatus = 1; // Pending
	m_Name = "";

	//Save our pet's new name to the db:
	SetPetNameForModeration(GeneralUtils::UTF16ToWTF8(name));

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);

	std::u16string u16name = GeneralUtils::UTF8ToUTF16(m_Name);
	std::u16string u16ownerName = GeneralUtils::UTF8ToUTF16(m_OwnerName);
	GameMessages::SendSetPetName(m_Tamer, u16name, m_DatabaseId, tamer->GetSystemAddress());
	GameMessages::SendSetPetName(m_Tamer, u16name, LWOOBJID_EMPTY, tamer->GetSystemAddress());
	GameMessages::SendPetNameChanged(m_ParentEntity->GetObjectID(), m_ModerationStatus, u16name, u16ownerName, UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::SendSetPetNameModerated(m_Tamer, m_DatabaseId, m_ModerationStatus, tamer->GetSystemAddress());

	GameMessages::SendNotifyPetTamingMinigame(
		m_Tamer,
		m_ParentEntity->GetObjectID(),
		m_Tamer,
		false,
		ePetTamingNotifyType::SUCCESS,
		NiPoint3::ZERO,
		NiPoint3::ZERO,
		NiQuaternion::IDENTITY,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	GameMessages::SendTerminateInteraction(m_Tamer, eTerminateType::FROM_INTERACTION, m_ParentEntity->GetObjectID());

	auto* modelEntity = EntityManager::Instance()->GetEntity(m_ModelId);

	if (modelEntity) modelEntity->Smash(m_Tamer);

	currentActivities.erase(m_Tamer);

	m_Tamer = LWOOBJID_EMPTY;

	// Notify the end of a pet taming minigame
	m_ParentEntity->GetScript()->OnNotifyPetTamingMinigame(m_ParentEntity, tamer, ePetTamingNotifyType::SUCCESS);

}

void PetComponent::ClientExitTamingMinigame(bool voluntaryExit) {
	if (m_Tamer == LWOOBJID_EMPTY) return;

	auto* tamer = EntityManager::Instance()->GetEntity(m_Tamer);

	if (!tamer) {
		m_Tamer = LWOOBJID_EMPTY;

		return;
	}

	GameMessages::SendNotifyPetTamingMinigame(
		m_Tamer,
		m_ParentEntity->GetObjectID(),
		m_Tamer,
		false,
		ePetTamingNotifyType::QUIT,
		NiPoint3::ZERO,
		NiPoint3::ZERO,
		NiQuaternion::IDENTITY,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	GameMessages::SendNotifyTamingModelLoadedOnServer(m_Tamer, tamer->GetSystemAddress());

	GameMessages::SendTerminateInteraction(m_Tamer, eTerminateType::FROM_INTERACTION, m_ParentEntity->GetObjectID());

	currentActivities.erase(m_Tamer);

	SetStatus(67108866);
	m_Tamer = LWOOBJID_EMPTY;
	m_Timer = 0;

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);

	// Notify the end of a pet taming minigame
	m_ParentEntity->GetScript()->OnNotifyPetTamingMinigame(m_ParentEntity, tamer, ePetTamingNotifyType::QUIT);

}

void PetComponent::StartTimer() {
	const auto& cached = buildCache.find(m_ParentEntity->GetLOT());

	if (cached != buildCache.end()) m_Timer = cached->second.timeLimit;
}

void PetComponent::ClientFailTamingMinigame() {
	if (m_Tamer == LWOOBJID_EMPTY) return;

	auto* tamer = EntityManager::Instance()->GetEntity(m_Tamer);

	if (!tamer) {
		m_Tamer = LWOOBJID_EMPTY;

		return;
	}

	GameMessages::SendNotifyPetTamingMinigame(
		m_Tamer,
		m_ParentEntity->GetObjectID(),
		m_Tamer,
		false,
		ePetTamingNotifyType::FAILED,
		NiPoint3::ZERO,
		NiPoint3::ZERO,
		NiQuaternion::IDENTITY,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	GameMessages::SendNotifyTamingModelLoadedOnServer(m_Tamer, tamer->GetSystemAddress());

	GameMessages::SendTerminateInteraction(m_Tamer, eTerminateType::FROM_INTERACTION, m_ParentEntity->GetObjectID());

	currentActivities.erase(m_Tamer);

	SetStatus(67108866);
	m_Tamer = LWOOBJID_EMPTY;
	m_Timer = 0;

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);

	// Notify the end of a pet taming minigame
	m_ParentEntity->GetScript()->OnNotifyPetTamingMinigame(m_ParentEntity, tamer, ePetTamingNotifyType::FAILED);

}

void PetComponent::Wander() {
	if (!m_MovementAI) m_MovementAI = m_ParentEntity->GetComponent<MovementAIComponent>();

	if (!m_MovementAI || !m_MovementAI->AtFinalWaypoint()) {
		return;
	}

	m_MovementAI->SetHaltDistance(0);

	const auto& info = m_MovementAI->GetInfo();

	const auto div = static_cast<int32_t>(info.wanderDelayMax);
	m_Timer = (div == 0 ? 0 : GeneralUtils::GenerateRandomNumber<int32_t>(0, div)) + info.wanderDelayMin; //set a random timer to stay put.

	const float radius = info.wanderRadius * sqrt(GeneralUtils::GenerateRandomNumber<float>(0.0f, 1.0f)); //our wander radius + a bit of random range
	const float theta = GeneralUtils::GenerateRandomNumber<float>(0.0f, 1.0f) * 2.0f * PI;

	const NiPoint3 delta = NiPoint3(
		radius * cos(theta),
		0,
		radius * sin(theta)
	);

	auto destination = m_StartPosition + delta;

	if (dpWorld::Instance().IsLoaded()) {
		destination.y = dpWorld::Instance().GetNavMesh()->GetHeightAtPoint(destination);
	}

	if (Vector3::DistanceSquared(destination, m_MovementAI->GetCurrentPosition()) < 2 * 2) {
		m_MovementAI->Stop();

		return;
	}

	m_MovementAI->SetSpeed(info.wanderSpeed);

	m_MovementAI->SetDestination(destination);

	m_Timer += (m_MovementAI->GetCurrentPosition().x - destination.x) / info.wanderSpeed;
}

void PetComponent::Activate(Item* item, bool registerPet, bool fromTaming) {
	AddDrainImaginationTimer(item, fromTaming);

	m_ItemId = item->GetId();
	m_DatabaseId = item->GetSubKey();

	auto inventoryComponent = item->GetInventory()->GetComponent();

	if (!inventoryComponent) return;

	inventoryComponent->DespawnPet();

	m_Owner = inventoryComponent->GetParentEntity()->GetObjectID();

	auto* owner = GetOwner();

	if (!owner) return;
	SetStatus(1);

	auto databaseData = inventoryComponent->GetDatabasePet(m_DatabaseId);

	m_ModerationStatus = databaseData.moderationState;

	bool updatedModerationStatus = false;

	//Load mod status from db:
	if (m_ModerationStatus != 2) {
		LoadPetNameFromModeration();

		databaseData.name = m_Name;
		databaseData.moderationState = m_ModerationStatus;

		inventoryComponent->SetDatabasePet(m_DatabaseId, databaseData);

		updatedModerationStatus = true;
	} else {
		m_Name = databaseData.name;
	}

	m_OwnerName = owner->GetCharacter()->GetName();

	if (updatedModerationStatus) {
		GameMessages::SendSetPetName(m_Owner, GeneralUtils::UTF8ToUTF16(m_Name), m_DatabaseId, owner->GetSystemAddress());
		GameMessages::SendSetPetNameModerated(m_Owner, m_DatabaseId, m_ModerationStatus, owner->GetSystemAddress());
	}

	GameMessages::SendMarkInventoryItemAsActive(m_Owner, true, eUnequippableActiveType::PET, m_ItemId, GetOwner()->GetSystemAddress());

	activePets[m_Owner] = m_ParentEntity->GetObjectID();

	m_Timer = 3;

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);

	owner->GetCharacter()->SetPlayerFlag(ePlayerFlag::FIRST_MANUAL_PET_HIBERNATE, true);

	if (registerPet) {
		GameMessages::SendAddPetToPlayer(m_Owner, 0, GeneralUtils::UTF8ToUTF16(m_Name), m_DatabaseId, m_ParentEntity->GetLOT(), owner->GetSystemAddress());

		GameMessages::SendRegisterPetID(m_Owner, m_ParentEntity->GetObjectID(), owner->GetSystemAddress());

		GameMessages::SendRegisterPetDBID(m_Owner, m_DatabaseId, owner->GetSystemAddress());
	}

	GameMessages::SendShowPetActionButton(m_Owner, 3, true, owner->GetSystemAddress());
}

void PetComponent::AddDrainImaginationTimer(Item* item, bool fromTaming) {
	if (Game::config->GetValue("pets_take_imagination") != "1") return;

	auto playerInventory = item->GetInventory();
	if (!playerInventory) return;

	auto playerInventoryComponent = playerInventory->GetComponent();
	if (!playerInventoryComponent) return;

	auto playerEntity = playerInventoryComponent->GetParentEntity();
	if (!playerEntity) return;

	auto* playerDestroyableComponent = playerEntity->GetComponent<DestroyableComponent>();
	if (!playerDestroyableComponent) return;

	// Drain by 1 when you summon pet or when this method is called, but not when we have just tamed this pet.
	if (!fromTaming) playerDestroyableComponent->Imagine(-1);

	// Set this to a variable so when this is called back from the player the timer doesn't fire off.
	m_ParentEntity->AddCallbackTimer(m_ImaginationDrainRate, [playerDestroyableComponent, this, item]() {
		if (!playerDestroyableComponent) {
			Game::logger->Log("PetComponent", "No petComponent and/or no playerDestroyableComponent");
			return;
		}

		// If we are out of imagination despawn the pet.
		if (playerDestroyableComponent->GetImagination() == 0) {
			this->Deactivate();
			auto playerEntity = playerDestroyableComponent->GetParentEntity();
			if (!playerEntity) return;

			GameMessages::SendUseItemRequirementsResponse(playerEntity->GetObjectID(), playerEntity->GetSystemAddress(), eUseItemResponse::NoImaginationForPet);
		}

		this->AddDrainImaginationTimer(item);
		});
}

void PetComponent::Deactivate() {
	GameMessages::SendPlayFXEffect(m_ParentEntity->GetObjectID(), -1, u"despawn", "", LWOOBJID_EMPTY, 1, 1, true);

	GameMessages::SendMarkInventoryItemAsActive(m_Owner, false, eUnequippableActiveType::PET, m_ItemId, GetOwner()->GetSystemAddress());

	activePets.erase(m_Owner);

	m_ParentEntity->Kill();

	auto* owner = GetOwner();

	if (!owner) return;

	GameMessages::SendAddPetToPlayer(m_Owner, 0, u"", LWOOBJID_EMPTY, LOT_NULL, owner->GetSystemAddress());

	GameMessages::SendRegisterPetID(m_Owner, LWOOBJID_EMPTY, owner->GetSystemAddress());

	GameMessages::SendRegisterPetDBID(m_Owner, LWOOBJID_EMPTY, owner->GetSystemAddress());

	GameMessages::SendShowPetActionButton(m_Owner, 0, false, owner->GetSystemAddress());
}

void PetComponent::Release() {
	auto* inventoryComponent = GetOwner()->GetComponent<InventoryComponent>();

	if (!inventoryComponent) return;

	Deactivate();

	inventoryComponent->RemoveDatabasePet(m_DatabaseId);

	auto* item = inventoryComponent->FindItemBySubKey(m_DatabaseId);

	item->SetCount(0, false, false);
}

void PetComponent::Command(NiPoint3 position, LWOOBJID source, int32_t commandType, int32_t typeId, bool overrideObey) {
	auto* owner = GetOwner();

	if (!owner) return;

	if (commandType == 1) {
		// Emotes
		GameMessages::SendPlayEmote(m_ParentEntity->GetObjectID(), typeId, owner->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
	} else if (commandType == 3) {
		// Follow me, ???
	} else if (commandType == 6) {
		// TODO: Go to player
	}

	if (owner->GetGMLevel() >= eGameMasterLevel::DEVELOPER) {
		ChatPackets::SendSystemMessage(owner->GetSystemAddress(), u"Command Type: " + (GeneralUtils::to_u16string(commandType)) + u" - Type Id: " + (GeneralUtils::to_u16string(typeId)));
	}
}

Entity* PetComponent::GetOwner() const {
	return EntityManager::Instance()->GetEntity(m_Owner);
}

PetComponent* PetComponent::GetTamingPet(const LWOOBJID& tamer) {
	const auto& pair = currentActivities.find(tamer);

	if (pair == currentActivities.end()) {
		return nullptr;
	}

	auto* entity = EntityManager::Instance()->GetEntity(pair->second);

	if (!entity) {
		currentActivities.erase(tamer);

		return nullptr;
	}

	return entity->GetComponent<PetComponent>();
}

PetComponent* PetComponent::GetActivePet(const LWOOBJID& owner) {
	const auto& pair = activePets.find(owner);

	if (pair == activePets.end()) {
		return nullptr;
	}

	auto* entity = EntityManager::Instance()->GetEntity(pair->second);

	if (!entity) {
		activePets.erase(owner);

		return nullptr;
	}

	return entity->GetComponent<PetComponent>();
}

void PetComponent::SetPetNameForModeration(const std::string& petName) {
	int approved = 1; //default, in mod

	//Make sure that the name isn't already auto-approved:
	if (Game::chatFilter->IsSentenceOkay(petName, eGameMasterLevel::CIVILIAN).empty()) {
		approved = 2; //approved
	}

	std::unique_ptr<sql::PreparedStatement> deleteStmt(Database::CreatePreppedStmt("DELETE FROM pet_names WHERE id = ? LIMIT 1;"));
	deleteStmt->setUInt64(1, m_DatabaseId);

	deleteStmt->execute();

	//Save to db:
	std::unique_ptr<sql::PreparedStatement> stmt(Database::CreatePreppedStmt("INSERT INTO `pet_names` (`id`, `pet_name`, `approved`) VALUES (?, ?, ?);"));
	stmt->setUInt64(1, m_DatabaseId);
	stmt->setString(2, petName);
	stmt->setInt(3, approved);
	stmt->execute();
}

void PetComponent::LoadPetNameFromModeration() {
	std::unique_ptr<sql::PreparedStatement> stmt(Database::CreatePreppedStmt("SELECT pet_name, approved FROM pet_names WHERE id = ? LIMIT 1;"));
	stmt->setUInt64(1, m_DatabaseId);

	std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
	if (!res->next()) return;
	m_ModerationStatus = res->getInt("approved");

	if (m_ModerationStatus == 2) {
		m_Name = res->getString("pet_name");
	}
}

void PetComponent::SetPreconditions(std::string& preconditions) {
	if (m_Preconditions) delete m_Preconditions;
	m_Preconditions = new PreconditionExpression(preconditions);
}
