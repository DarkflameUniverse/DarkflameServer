#include "PetComponent.h"
#include "GameMessages.h"
#include "BrickDatabase.h"
#include "CDClientDatabase.h"
#include "CDTamingBuildPuzzleTable.h"
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
#include "ObjectIDManager.h"
#include "eUnequippableActiveType.h"
#include "eTerminateType.h"
#include "ePetTamingNotifyType.h"
#include "eUseItemResponse.h"
#include "ePlayerFlag.h"

#include "Game.h"
#include "dConfig.h"
#include "dChatFilter.h"
#include "Database.h"
#include "EntityInfo.h"
#include "eMissionTaskType.h"
#include "RenderComponent.h"
#include "eObjectBits.h"
#include "eGameMasterLevel.h"
#include "eMissionState.h"
#include "dNavMesh.h"
#include "eGameActivity.h"
#include "eStateChangeType.h"

std::unordered_map<LWOOBJID, LWOOBJID> PetComponent::currentActivities{};
std::unordered_map<LWOOBJID, LWOOBJID> PetComponent::activePets{};

/**
 * Maps all the pet lots to a flag indicating that the player has caught it. All basic pets have been guessed by ObjID
 * while the faction ones could be checked using their respective missions.
 */
const std::map<LOT, int32_t> PetComponent::petFlags{
		{ 3050, 801 },  // Elephant
		{ 3054, 803 },  // Cat
		{ 3195, 806 },  // Triceratops
		{ 3254, 807 },  // Terrier
		{ 3261, 811 },  // Skunk
		{ 3672, 813 },  // Bunny
		{ 3994, 814 },  // Crocodile
		{ 5635, 815 },  // Doberman
		{ 5636, 816 },  // Buffalo
		{ 5637, 818 },  // Robot Dog
		{ 5639, 819 },  // Red Dragon
		{ 5640, 820 },  // Tortoise
		{ 5641, 821 },  // Green Dragon
		{ 5643, 822 },  // Panda, see mission 786
		{ 5642, 823 },  // Mantis
		{ 6720, 824 },  // Warthog
		{ 3520, 825 },  // Lion, see mission 1318
		{ 7638, 826 },  // Goat
		{ 7694, 827 },  // Crab
		{ 12294, 829 }, // Reindeer
		{ 12431, 830 }, // Stegosaurus, see mission 1386
		{ 12432, 831 }, // Saber cat, see mission 1389
		{ 12433, 832 }, // Gryphon, see mission 1392
		{ 12434, 833 }, // Alien, see mission 1188
		// 834: unknown?, see mission 506, 688
		{ 16210, 836 },  // Ninjago Earth Dragon, see mission 1836
		{ 13067, 838 }, // Skeleton dragon
};

PetComponent::PetComponent(Entity* parentEntity, uint32_t componentId) : Component{ parentEntity } {
	m_PetInfo = CDClientManager::GetTable<CDPetComponentTable>()->GetByID(componentId); // TODO: Make reference when safe
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
	m_Ability = ePetAbilityType::Invalid;
	m_StartPosition = NiPoint3Constant::ZERO;
	m_MovementAI = nullptr;
	m_TresureTime = 0;

	std::string checkPreconditions = GeneralUtils::UTF16ToWTF8(parentEntity->GetVar<std::u16string>(u"CheckPrecondition"));

	if (!checkPreconditions.empty()) {
		SetPreconditions(checkPreconditions);
	}
}

void PetComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	const bool tamed = m_Owner != LWOOBJID_EMPTY;

	outBitStream.Write1(); // Always serialize as dirty for now

	outBitStream.Write<uint32_t>(m_Status);
	outBitStream.Write(tamed ? m_Ability : ePetAbilityType::Invalid); // Something with the overhead icon?

	const bool interacting = m_Interaction != LWOOBJID_EMPTY;

	outBitStream.Write(interacting);
	if (interacting) {
		outBitStream.Write(m_Interaction);
	}

	outBitStream.Write(tamed);
	if (tamed) {
		outBitStream.Write(m_Owner);
	}

	if (bIsInitialUpdate) {
		outBitStream.Write(tamed);
		if (tamed) {
			outBitStream.Write(m_ModerationStatus);

			const auto nameData = GeneralUtils::UTF8ToUTF16(m_Name);
			const auto ownerNameData = GeneralUtils::UTF8ToUTF16(m_OwnerName);

			outBitStream.Write<uint8_t>(nameData.size());
			for (const auto c : nameData) {
				outBitStream.Write(c);
			}

			outBitStream.Write<uint8_t>(ownerNameData.size());
			for (const auto c : ownerNameData) {
				outBitStream.Write(c);
			}
		}
	}
}

void PetComponent::OnUse(Entity* originator) {
	if (m_Owner != LWOOBJID_EMPTY) {
		return;
	}

	if (m_Tamer != LWOOBJID_EMPTY) {
		auto* tamer = Game::entityManager->GetEntity(m_Tamer);

		if (tamer != nullptr) {
			return;
		}

		m_Tamer = LWOOBJID_EMPTY;
	}

	auto* const inventoryComponent = originator->GetComponent<InventoryComponent>();
	if (inventoryComponent == nullptr) {
		return;
	}

	if (m_Preconditions.has_value() && !m_Preconditions->Check(originator, true)) {
		return;
	}

	auto* const movementAIComponent = m_Parent->GetComponent<MovementAIComponent>();
	if (movementAIComponent != nullptr) {
		movementAIComponent->Stop();
	}

	inventoryComponent->DespawnPet();

	const auto* const entry = CDClientManager::GetTable<CDTamingBuildPuzzleTable>()->GetByLOT(m_Parent->GetLOT());
	if (!entry) {
		ChatPackets::SendSystemMessage(originator->GetSystemAddress(), u"Failed to find the puzzle minigame for this pet.");
		return;
	}

	const auto* const destroyableComponent = originator->GetComponent<DestroyableComponent>();
	if (destroyableComponent == nullptr) {
		return;
	}

	const auto imagination = destroyableComponent->GetImagination();
	if (imagination < entry->imaginationCost) {
		return;
	}

	const auto& bricks = BrickDatabase::GetBricks(entry->validPieces);
	if (bricks.empty()) {
		ChatPackets::SendSystemMessage(originator->GetSystemAddress(), u"Failed to load the puzzle minigame for this pet.");
		LOG("Couldn't find %s for minigame!", entry->validPieces.c_str());

		return;
	}

	const auto petPosition = m_Parent->GetPosition();

	const auto originatorPosition = originator->GetPosition();

	m_Parent->SetRotation(NiQuaternion::LookAt(petPosition, originatorPosition));

	float interactionDistance = m_Parent->GetVar<float>(u"interaction_distance");
	if (interactionDistance <= 0) {
		interactionDistance = 15;
	}

	auto position = originatorPosition;

	NiPoint3 forward = NiQuaternion::LookAt(m_Parent->GetPosition(), originator->GetPosition()).GetForwardVector();
	forward.y = 0;

	if (dpWorld::IsLoaded()) {
		NiPoint3 attempt = petPosition + forward * interactionDistance;

		NiPoint3 nearestPoint = dpWorld::GetNavMesh()->NearestPoint(attempt);

		while (std::abs(nearestPoint.y - petPosition.y) > 4 && interactionDistance > 10) {
			const NiPoint3 forward = m_Parent->GetRotation().GetForwardVector();

			attempt = originatorPosition + forward * interactionDistance;

			nearestPoint = dpWorld::GetNavMesh()->NearestPoint(attempt);

			interactionDistance -= 0.5f;
		}

		position = nearestPoint;
	} else {
		position = petPosition + forward * interactionDistance;
	}

	auto rotation = NiQuaternion::LookAt(position, petPosition);

	GameMessages::SendNotifyPetTamingMinigame(
		originator->GetObjectID(),
		m_Parent->GetObjectID(),
		LWOOBJID_EMPTY,
		true,
		ePetTamingNotifyType::BEGIN,
		petPosition,
		position,
		rotation,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	GameMessages::SendNotifyPetTamingMinigame(
		m_Parent->GetObjectID(),
		LWOOBJID_EMPTY,
		originator->GetObjectID(),
		false,
		ePetTamingNotifyType::BEGIN,
		NiPoint3Constant::ZERO,
		NiPoint3Constant::ZERO,
		NiQuaternion(0.0f, 0.0f, 0.0f, 0.0f),
		UNASSIGNED_SYSTEM_ADDRESS
	);

	GameMessages::SendNotifyPetTamingPuzzleSelected(originator->GetObjectID(), bricks, originator->GetSystemAddress());

	m_Tamer = originator->GetObjectID();
	SetStatus(5);
	Game::entityManager->SerializeEntity(m_Parent);

	currentActivities.insert_or_assign(m_Tamer, m_Parent->GetObjectID());

	// Notify the start of a pet taming minigame
	m_Parent->GetScript()->OnNotifyPetTamingMinigame(m_Parent, originator, ePetTamingNotifyType::BEGIN);

	auto* characterComponent = originator->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->SetCurrentActivity(eGameActivity::PET_TAMING);
		Game::entityManager->SerializeEntity(originator);
	}
}

void PetComponent::Update(float deltaTime) {
	if (m_StartPosition == NiPoint3Constant::ZERO) {
		m_StartPosition = m_Parent->GetPosition();
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
					Game::entityManager->SerializeEntity(m_Parent);
				}
			} else {
				m_Timer = 5;
			}
		}

		return;
	}

	auto* owner = GetOwner();

	if (owner == nullptr) {
		m_Parent->Kill();

		return;
	}

	m_MovementAI = m_Parent->GetComponent<MovementAIComponent>();

	if (m_MovementAI == nullptr) {
		return;
	}

	if (m_TresureTime > 0) {
		auto* tresure = Game::entityManager->GetEntity(m_Interaction);

		if (tresure == nullptr) {
			m_TresureTime = 0;

			return;
		}

		m_TresureTime -= deltaTime;

		m_MovementAI->Stop();

		if (m_TresureTime <= 0) {
			m_Parent->SetOwnerOverride(m_Owner);

			tresure->Smash(m_Parent->GetObjectID());

			m_Interaction = LWOOBJID_EMPTY;

			m_TresureTime = 0;
		}

		return;
	}

	auto destination = owner->GetPosition();
	NiPoint3 position = m_MovementAI->GetParent()->GetPosition();

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

	SwitchComponent* closestSwitch = SwitchComponent::GetClosestSwitch(position);

	float haltDistance = 5;

	if (closestSwitch != nullptr) {
		if (!closestSwitch->GetActive()) {
			NiPoint3 switchPosition = closestSwitch->GetParentEntity()->GetPosition();
			float distance = Vector3::DistanceSquared(position, switchPosition);
			if (distance < 3 * 3) {
				m_Interaction = closestSwitch->GetParentEntity()->GetObjectID();
				closestSwitch->EntityEnter(m_Parent);
			} else if (distance < 20 * 20) {
				haltDistance = 1;

				destination = switchPosition;
			}
		}
	}

	auto* missionComponent = owner->GetComponent<MissionComponent>();
	if (!missionComponent) return;

	// Determine if the "Lost Tags" mission has been completed and digging has been unlocked
	const bool digUnlocked = missionComponent->GetMissionState(842) == eMissionState::COMPLETE;

	Entity* closestTresure = PetDigServer::GetClosestTresure(position);

	if (closestTresure != nullptr && digUnlocked) {
		// Skeleton Dragon Pat special case for bone digging
		if (closestTresure->GetLOT() == 12192 && m_Parent->GetLOT() != 13067) {
			goto skipTresure;
		}

		NiPoint3 tresurePosition = closestTresure->GetPosition();
		float distance = Vector3::DistanceSquared(position, tresurePosition);
		if (distance < 5 * 5) {
			m_Interaction = closestTresure->GetObjectID();

			Command(NiPoint3Constant::ZERO, LWOOBJID_EMPTY, 1, 202, true);

			m_TresureTime = 2;
		} else if (distance < 10 * 10) {
			haltDistance = 1;

			destination = tresurePosition;
		}
	}

skipTresure:

	m_MovementAI->SetHaltDistance(haltDistance);

	m_MovementAI->SetMaxSpeed(2.5f);

	m_MovementAI->SetDestination(destination);

	m_Timer = 1;
}

void PetComponent::TryBuild(uint32_t numBricks, bool clientFailed) {
	if (m_Tamer == LWOOBJID_EMPTY) return;

	auto* tamer = Game::entityManager->GetEntity(m_Tamer);

	if (tamer == nullptr) {
		m_Tamer = LWOOBJID_EMPTY;

		return;
	}

	const auto* const entry = CDClientManager::GetTable<CDTamingBuildPuzzleTable>()->GetByLOT(m_Parent->GetLOT());
	if (!entry) return;

	auto* destroyableComponent = tamer->GetComponent<DestroyableComponent>();

	if (destroyableComponent == nullptr) return;

	auto imagination = destroyableComponent->GetImagination();

	imagination -= entry->imaginationCost;

	destroyableComponent->SetImagination(imagination);

	Game::entityManager->SerializeEntity(tamer);

	if (clientFailed) {
		if (imagination < entry->imaginationCost) {
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

	auto* tamer = Game::entityManager->GetEntity(m_Tamer);

	if (tamer == nullptr) {
		m_Tamer = LWOOBJID_EMPTY;

		return;
	}

	const auto* const entry = CDClientManager::GetTable<CDTamingBuildPuzzleTable>()->GetByLOT(m_Parent->GetLOT());
	if (!entry) return;

	GameMessages::SendPlayFXEffect(tamer, -1, u"petceleb", "", LWOOBJID_EMPTY, 1, 1, true);
	RenderComponent::PlayAnimation(tamer, u"rebuild-celebrate");

	EntityInfo info{};
	info.lot = entry->puzzleModelLot;
	info.pos = position;
	info.rot = NiQuaternionConstant::IDENTITY;
	info.spawnerID = tamer->GetObjectID();

	auto* modelEntity = Game::entityManager->CreateEntity(info);

	m_ModelId = modelEntity->GetObjectID();

	Game::entityManager->ConstructEntity(modelEntity);

	GameMessages::SendNotifyTamingModelLoadedOnServer(m_Tamer, tamer->GetSystemAddress());

	GameMessages::SendPetResponse(m_Tamer, m_Parent->GetObjectID(), 0, 10, 0, tamer->GetSystemAddress());

	auto* inventoryComponent = tamer->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	LWOOBJID petSubKey = ObjectIDManager::GenerateRandomObjectID();

	GeneralUtils::SetBit(petSubKey, eObjectBits::CHARACTER);
	GeneralUtils::SetBit(petSubKey, eObjectBits::PERSISTENT);

	m_DatabaseId = petSubKey;

	std::string petName = tamer->GetCharacter()->GetName();
	petName += "'s Pet";

	GameMessages::SendAddPetToPlayer(m_Tamer, 0, GeneralUtils::UTF8ToUTF16(petName), petSubKey, m_Parent->GetLOT(), tamer->GetSystemAddress());

	GameMessages::SendRegisterPetID(m_Tamer, m_Parent->GetObjectID(), tamer->GetSystemAddress());

	GameMessages::SendRegisterPetDBID(m_Tamer, petSubKey, tamer->GetSystemAddress());

	inventoryComponent->AddItem(m_Parent->GetLOT(), 1, eLootSourceType::ACTIVITY, eInventoryType::MODELS, {}, LWOOBJID_EMPTY, true, false, petSubKey);
	auto* item = inventoryComponent->FindItemBySubKey(petSubKey, MODELS);

	if (item == nullptr) {
		return;
	}

	DatabasePet databasePet{};

	databasePet.lot = m_Parent->GetLOT();
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
		NiPoint3Constant::ZERO,
		NiPoint3Constant::ZERO,
		NiQuaternionConstant::IDENTITY,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	// Triggers the catch a pet missions
	if (petFlags.find(m_Parent->GetLOT()) != petFlags.end()) {
		tamer->GetCharacter()->SetPlayerFlag(petFlags.at(m_Parent->GetLOT()), true);
	}

	auto* missionComponent = tamer->GetComponent<MissionComponent>();

	if (missionComponent != nullptr) {
		missionComponent->Progress(eMissionTaskType::PET_TAMING, m_Parent->GetLOT());
	}

	SetStatus(1);

	auto* characterComponent = tamer->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
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

	auto* tamer = Game::entityManager->GetEntity(m_Tamer);

	if (tamer == nullptr) {
		m_Tamer = LWOOBJID_EMPTY;

		return;
	}

	LOG("Got set pet name (%s)", GeneralUtils::UTF16ToWTF8(name).c_str());

	auto* inventoryComponent = tamer->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	m_ModerationStatus = 1; // Pending
	m_Name = "";

	//Save our pet's new name to the db:
	SetPetNameForModeration(GeneralUtils::UTF16ToWTF8(name));

	Game::entityManager->SerializeEntity(m_Parent);

	std::u16string u16name = GeneralUtils::UTF8ToUTF16(m_Name);
	std::u16string u16ownerName = GeneralUtils::UTF8ToUTF16(m_OwnerName);
	GameMessages::SendSetPetName(m_Tamer, u16name, m_DatabaseId, tamer->GetSystemAddress());
	GameMessages::SendSetPetName(m_Tamer, u16name, LWOOBJID_EMPTY, tamer->GetSystemAddress());
	GameMessages::SendPetNameChanged(m_Parent->GetObjectID(), m_ModerationStatus, u16name, u16ownerName, UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::SendSetPetNameModerated(m_Tamer, m_DatabaseId, m_ModerationStatus, tamer->GetSystemAddress());

	GameMessages::SendNotifyPetTamingMinigame(
		m_Tamer,
		m_Parent->GetObjectID(),
		m_Tamer,
		false,
		ePetTamingNotifyType::SUCCESS,
		NiPoint3Constant::ZERO,
		NiPoint3Constant::ZERO,
		NiQuaternionConstant::IDENTITY,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	auto* characterComponent = tamer->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->SetCurrentActivity(eGameActivity::NONE);
		Game::entityManager->SerializeEntity(tamer);
	}
	GameMessages::SendTerminateInteraction(m_Tamer, eTerminateType::FROM_INTERACTION, m_Parent->GetObjectID());

	auto* modelEntity = Game::entityManager->GetEntity(m_ModelId);

	if (modelEntity != nullptr) {
		modelEntity->Smash(m_Tamer);
	}

	currentActivities.erase(m_Tamer);

	m_Tamer = LWOOBJID_EMPTY;

	// Notify the end of a pet taming minigame
	m_Parent->GetScript()->OnNotifyPetTamingMinigame(m_Parent, tamer, ePetTamingNotifyType::SUCCESS);
}

void PetComponent::ClientExitTamingMinigame(bool voluntaryExit) {
	if (m_Tamer == LWOOBJID_EMPTY) return;

	auto* tamer = Game::entityManager->GetEntity(m_Tamer);

	if (tamer == nullptr) {
		m_Tamer = LWOOBJID_EMPTY;

		return;
	}

	GameMessages::SendNotifyPetTamingMinigame(
		m_Tamer,
		m_Parent->GetObjectID(),
		m_Tamer,
		false,
		ePetTamingNotifyType::QUIT,
		NiPoint3Constant::ZERO,
		NiPoint3Constant::ZERO,
		NiQuaternionConstant::IDENTITY,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	auto* characterComponent = tamer->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->SetCurrentActivity(eGameActivity::NONE);
		Game::entityManager->SerializeEntity(tamer);
	}
	GameMessages::SendNotifyTamingModelLoadedOnServer(m_Tamer, tamer->GetSystemAddress());

	GameMessages::SendTerminateInteraction(m_Tamer, eTerminateType::FROM_INTERACTION, m_Parent->GetObjectID());

	currentActivities.erase(m_Tamer);

	SetStatus(67108866);
	m_Tamer = LWOOBJID_EMPTY;
	m_Timer = 0;

	Game::entityManager->SerializeEntity(m_Parent);

	// Notify the end of a pet taming minigame
	m_Parent->GetScript()->OnNotifyPetTamingMinigame(m_Parent, tamer, ePetTamingNotifyType::QUIT);
}

void PetComponent::StartTimer() {
	const auto* const entry = CDClientManager::GetTable<CDTamingBuildPuzzleTable>()->GetByLOT(m_Parent->GetLOT());
	if (!entry) return;

	m_Timer = entry->timeLimit;
}

void PetComponent::ClientFailTamingMinigame() {
	if (m_Tamer == LWOOBJID_EMPTY) return;

	auto* tamer = Game::entityManager->GetEntity(m_Tamer);

	if (tamer == nullptr) {
		m_Tamer = LWOOBJID_EMPTY;

		return;
	}

	GameMessages::SendNotifyPetTamingMinigame(
		m_Tamer,
		m_Parent->GetObjectID(),
		m_Tamer,
		false,
		ePetTamingNotifyType::FAILED,
		NiPoint3Constant::ZERO,
		NiPoint3Constant::ZERO,
		NiQuaternionConstant::IDENTITY,
		UNASSIGNED_SYSTEM_ADDRESS
	);

	auto* characterComponent = tamer->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->SetCurrentActivity(eGameActivity::NONE);
		Game::entityManager->SerializeEntity(tamer);
	}
	GameMessages::SendNotifyTamingModelLoadedOnServer(m_Tamer, tamer->GetSystemAddress());

	GameMessages::SendTerminateInteraction(m_Tamer, eTerminateType::FROM_INTERACTION, m_Parent->GetObjectID());

	currentActivities.erase(m_Tamer);

	SetStatus(67108866);
	m_Tamer = LWOOBJID_EMPTY;
	m_Timer = 0;

	Game::entityManager->SerializeEntity(m_Parent);

	// Notify the end of a pet taming minigame
	m_Parent->GetScript()->OnNotifyPetTamingMinigame(m_Parent, tamer, ePetTamingNotifyType::FAILED);
}

void PetComponent::Wander() {
	m_MovementAI = m_Parent->GetComponent<MovementAIComponent>();

	if (m_MovementAI == nullptr || !m_MovementAI->AtFinalWaypoint()) {
		return;
	}

	m_MovementAI->SetHaltDistance(0);

	const auto& info = m_MovementAI->GetInfo();

	const auto div = static_cast<int>(info.wanderDelayMax);
	m_Timer = (div == 0 ? 0 : GeneralUtils::GenerateRandomNumber<int>(0, div)) + info.wanderDelayMin; //set a random timer to stay put.

	const float radius = info.wanderRadius * sqrt(static_cast<double>(GeneralUtils::GenerateRandomNumber<float>(0, 1))); //our wander radius + a bit of random range
	const float theta = ((static_cast<double>(GeneralUtils::GenerateRandomNumber<float>(0, 1)) * 2 * PI));

	const NiPoint3 delta =
	{
		radius * cos(theta),
		0,
		radius * sin(theta)
	};

	auto destination = m_StartPosition + delta;

	if (dpWorld::IsLoaded()) {
		destination.y = dpWorld::GetNavMesh()->GetHeightAtPoint(destination);
	}

	if (Vector3::DistanceSquared(destination, m_MovementAI->GetParent()->GetPosition()) < 2 * 2) {
		m_MovementAI->Stop();

		return;
	}

	m_MovementAI->SetMaxSpeed(m_PetInfo.sprintSpeed);

	m_MovementAI->SetDestination(destination);

	m_Timer += (m_MovementAI->GetParent()->GetPosition().x - destination.x) / m_PetInfo.sprintSpeed;
}

void PetComponent::Activate(Item* item, bool registerPet, bool fromTaming) {
	AddDrainImaginationTimer(item, fromTaming);

	m_ItemId = item->GetId();
	m_DatabaseId = item->GetSubKey();

	auto* inventoryComponent = item->GetInventory()->GetComponent();

	if (inventoryComponent == nullptr) return;

	inventoryComponent->DespawnPet();

	m_Owner = inventoryComponent->GetParent()->GetObjectID();

	auto* owner = GetOwner();

	if (owner == nullptr) return;
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

	activePets[m_Owner] = m_Parent->GetObjectID();

	m_Timer = 3;

	Game::entityManager->SerializeEntity(m_Parent);

	owner->GetCharacter()->SetPlayerFlag(ePlayerFlag::FIRST_MANUAL_PET_HIBERNATE, true);

	if (registerPet) {
		GameMessages::SendAddPetToPlayer(m_Owner, 0, GeneralUtils::UTF8ToUTF16(m_Name), m_DatabaseId, m_Parent->GetLOT(), owner->GetSystemAddress());

		GameMessages::SendRegisterPetID(m_Owner, m_Parent->GetObjectID(), owner->GetSystemAddress());

		GameMessages::SendRegisterPetDBID(m_Owner, m_DatabaseId, owner->GetSystemAddress());
	}
}

void PetComponent::AddDrainImaginationTimer(Item* item, bool fromTaming) {
	if (Game::config->GetValue("pets_take_imagination") != "1") return;

	auto playerInventory = item->GetInventory();
	if (!playerInventory) return;

	auto playerInventoryComponent = playerInventory->GetComponent();
	if (!playerInventoryComponent) return;

	auto playerEntity = playerInventoryComponent->GetParent();
	if (!playerEntity) return;

	auto playerDestroyableComponent = playerEntity->GetComponent<DestroyableComponent>();
	if (!playerDestroyableComponent) return;

	// Drain by 1 when you summon pet or when this method is called, but not when we have just tamed this pet.
	if (!fromTaming) playerDestroyableComponent->Imagine(-1);

	// Set this to a variable so when this is called back from the player the timer doesn't fire off.
	m_Parent->AddCallbackTimer(m_PetInfo.imaginationDrainRate, [playerDestroyableComponent, this, item]() {
		if (!playerDestroyableComponent) {
			LOG("No petComponent and/or no playerDestroyableComponent");
			return;
		}

		// If we are out of imagination despawn the pet.
		if (playerDestroyableComponent->GetImagination() == 0) {
			this->Deactivate();
			auto playerEntity = playerDestroyableComponent->GetParent();
			if (!playerEntity) return;

			GameMessages::SendUseItemRequirementsResponse(playerEntity->GetObjectID(), playerEntity->GetSystemAddress(), eUseItemResponse::NoImaginationForPet);
		}

		this->AddDrainImaginationTimer(item);
		});
}

void PetComponent::Deactivate() {
	GameMessages::SendPlayFXEffect(m_Parent->GetObjectID(), -1, u"despawn", "", LWOOBJID_EMPTY, 1, 1, true);

	GameMessages::SendMarkInventoryItemAsActive(m_Owner, false, eUnequippableActiveType::PET, m_ItemId, GetOwner()->GetSystemAddress());

	activePets.erase(m_Owner);

	m_Parent->Kill();

	auto* owner = GetOwner();

	if (owner == nullptr) return;

	GameMessages::SendAddPetToPlayer(m_Owner, 0, u"", LWOOBJID_EMPTY, LOT_NULL, owner->GetSystemAddress());

	GameMessages::SendRegisterPetID(m_Owner, LWOOBJID_EMPTY, owner->GetSystemAddress());

	GameMessages::SendRegisterPetDBID(m_Owner, LWOOBJID_EMPTY, owner->GetSystemAddress());

	GameMessages::SendShowPetActionButton(m_Owner, ePetAbilityType::Invalid, false, owner->GetSystemAddress());
}

void PetComponent::Release() {
	auto* inventoryComponent = GetOwner()->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	Deactivate();

	inventoryComponent->RemoveDatabasePet(m_DatabaseId);

	auto* item = inventoryComponent->FindItemBySubKey(m_DatabaseId);

	item->SetCount(0, false, false);
}

void PetComponent::Command(const NiPoint3& position, const LWOOBJID source, const int32_t commandType, const int32_t typeId, const bool overrideObey) {
	auto* owner = GetOwner();
	if (!owner) return;

	if (commandType == 1) {
		// Emotes
		GameMessages::SendPlayEmote(m_Parent->GetObjectID(), typeId, owner->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
	} else if (commandType == 3) {
		// Follow me, ???
	} else if (commandType == 6) {
		// TODO: Go to player
	}

	if (owner->GetGMLevel() >= eGameMasterLevel::DEVELOPER) {
		ChatPackets::SendSystemMessage(owner->GetSystemAddress(), u"Commmand Type: " + (GeneralUtils::to_u16string(commandType)) + u" - Type Id: " + (GeneralUtils::to_u16string(typeId)));
	}
}

LWOOBJID PetComponent::GetOwnerId() const {
	return m_Owner;
}

Entity* PetComponent::GetOwner() const {
	return Game::entityManager->GetEntity(m_Owner);
}

LWOOBJID PetComponent::GetDatabaseId() const {
	return m_DatabaseId;
}

LWOOBJID PetComponent::GetInteraction() const {
	return m_Interaction;
}

LWOOBJID PetComponent::GetItemId() const {
	return m_ItemId;
}

uint32_t PetComponent::GetStatus() const {
	return m_Status;
}

ePetAbilityType PetComponent::GetAbility() const {
	return m_Ability;
}

void PetComponent::SetInteraction(LWOOBJID value) {
	m_Interaction = value;
}

void PetComponent::SetStatus(uint32_t value) {
	m_Status = value;
}

void PetComponent::SetAbility(ePetAbilityType value) {
	m_Ability = value;
}

PetComponent* PetComponent::GetTamingPet(LWOOBJID tamer) {
	const auto& pair = currentActivities.find(tamer);

	if (pair == currentActivities.end()) {
		return nullptr;
	}

	auto* entity = Game::entityManager->GetEntity(pair->second);

	if (entity == nullptr) {
		currentActivities.erase(tamer);

		return nullptr;
	}

	return entity->GetComponent<PetComponent>();
}

PetComponent* PetComponent::GetActivePet(LWOOBJID owner) {
	const auto& pair = activePets.find(owner);

	if (pair == activePets.end()) {
		return nullptr;
	}

	auto* entity = Game::entityManager->GetEntity(pair->second);

	if (entity == nullptr) {
		activePets.erase(owner);

		return nullptr;
	}

	return entity->GetComponent<PetComponent>();
}

Entity* PetComponent::GetParentEntity() const {
	return m_Parent;
}

PetComponent::~PetComponent() {
}

void PetComponent::SetPetNameForModeration(const std::string& petName) {
	int approved = 1; //default, in mod

	//Make sure that the name isn't already auto-approved:
	if (Game::chatFilter->IsSentenceOkay(petName, eGameMasterLevel::CIVILIAN).empty()) {
		approved = 2; //approved
	}

	//Save to db:
	Database::Get()->SetPetNameModerationStatus(m_DatabaseId, IPetNames::Info{ petName, approved });
}

void PetComponent::LoadPetNameFromModeration() {
	auto petNameInfo = Database::Get()->GetPetNameInfo(m_DatabaseId);
	if (petNameInfo) {
		m_ModerationStatus = petNameInfo->approvalStatus;
		if (m_ModerationStatus == 2) {
			m_Name = petNameInfo->petName;
		}
	}
}

void PetComponent::SetPreconditions(const std::string& preconditions) {
	m_Preconditions = std::make_optional<PreconditionExpression>(preconditions);
}
