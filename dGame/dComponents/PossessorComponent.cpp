#include "PossessorComponent.h"
#include "PossessableComponent.h"
#include "CharacterComponent.h"
#include "DestroyableComponent.h"
#include "ModuleAssemblyComponent.h"
#include "VehiclePhysicsComponent.h"
#include "EntityManager.h"
#include "GameMessages.h"


PossessorComponent::PossessorComponent(Entity* parent) : Component(parent){
	m_Possessable = LWOOBJID_EMPTY;
	m_PossessableType = ePossessionType::NO_POSSESSION;
}

void PossessorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_DirtyPossesor);
	if (m_DirtyPossesor) {
		outBitStream->Write(m_Possessable != LWOOBJID_EMPTY);
		if (m_Possessable != LWOOBJID_EMPTY) {
			outBitStream->Write(m_Possessable);
		}
		outBitStream->Write(m_PossessableType);
	}
}

void PossessorComponent::Mount(Item* item) {
	auto* current = EntityManager::Instance()->GetEntity(GetPossessable());

	//
	if (current){
		Dismount(GetPossesableItem());
		if (GetPossesableItem() == item){
			SetPossesableItem(nullptr);
			return;
		}
	}
	// Need to wait for dismounting to be complete to be able to mount somehting else
	if (GetIsDismounting()) return;

	SetPossesableItem(item);
	const auto type = static_cast<eItemType>(item->GetInfo().itemType);

	if (type == ITEM_TYPE_MOUNT){
		// spawn the mount
		EntityInfo info{};

		info.lot = item->GetLot();
		info.spawnerID = 1;
		info.pos = m_Parent->GetPosition();
		info.rot = m_Parent->GetRotation();

		auto* mount = EntityManager::Instance()->CreateEntity(info, nullptr, m_Parent);
		
		// if we have a vehicle physics component, we need to do some extra work
		if (mount->HasComponent(COMPONENT_TYPE_VEHICLE_PHYSICS)) {
			Game::logger->Log("Possessor", "Mounting a mount that is a vehicle\n");
			// Get the position and rotation of the player to spawn the vehicle
			// and then invert it
			// otherwise, we'll be upside down, thanks
			// TODO: Make vehicle mounts not be upside down
		}

		// possess it
		auto* possessable = mount->GetComponent<PossessableComponent>();
		if (possessable) {
			possessable->SetPossessor(m_Parent->GetObjectID());
			const auto animationFlag = static_cast<eAnimationFlags>(item->GetInfo().animationFlag);
			possessable->SetAnimationFlag(animationFlag);
		}
		SetPossessable(mount->GetObjectID());
		SetPossessableType(mount->GetComponent<PossessableComponent>()->GetPossessionType());

		// only send this on mounting
		GameMessages::SendSetMountInventoryID(m_Parent, mount->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		// make it exist
		EntityManager::Instance()->ConstructEntity(mount);
		// stun the player
		GameMessages::SendSetStunned(m_Parent->GetObjectID(), eStunState::PUSH, m_Parent->GetSystemAddress(), LWOOBJID_EMPTY, true, false, true, false, false, false, false, true, true, true, true, true, true, true, true, true);

	} else if (type == ITEM_TYPE_VEHICLE && m_Parent->GetGMLevel() >= GAME_MASTER_LEVEL_OPERATOR){
		// This is only for GM's only

		// Get the position and rotation of the player to spawn the vehicle
		// otherwise, we'll be upside down, thanks
		auto startPosition = m_Parent->GetPosition();
		auto startRotation = NiQuaternion::LookAt(startPosition, startPosition + NiPoint3::UNIT_X);
		auto angles = startRotation.GetEulerAngles();
		angles.y -= PI;
		startRotation = NiQuaternion::FromEulerAngles(angles);

		GameMessages::SendTeleport(m_Parent->GetObjectID(), startPosition, startRotation, m_Parent->GetSystemAddress(), true, true);

		EntityInfo info {};
		info.lot = item->GetLot();
		info.pos = startPosition;
		info.rot = startRotation;
		info.spawnerID = m_Parent->GetObjectID();

		auto* carEntity = EntityManager::Instance()->CreateEntity(info, nullptr, m_Parent);
		m_Parent->AddChild(carEntity);

		auto *destroyableComponent = carEntity->GetComponent<DestroyableComponent>();

		// Setup the vehicle stats.
		if (destroyableComponent != nullptr) {
			destroyableComponent->SetIsSmashable(false);
			destroyableComponent->SetIsImmune(true);
		}

		// Possess the car
		auto* possessableComponent = carEntity->GetComponent<PossessableComponent>();

		if (possessableComponent != nullptr) {
			possessableComponent->SetPossessor(m_Parent->GetObjectID());
		}

		// build it
		auto* moduleAssemblyComponent = carEntity->GetComponent<ModuleAssemblyComponent>();

		if (moduleAssemblyComponent != nullptr) {
			moduleAssemblyComponent->SetSubKey(item->GetSubKey());
			moduleAssemblyComponent->SetUseOptionalParts(false);

			for (auto* config : item->GetConfig()) {
				if (config->GetKey() == u"assemblyPartLOTs") {
					moduleAssemblyComponent->SetAssemblyPartsLOTs(GeneralUtils::ASCIIToUTF16(config->GetValueAsString()));
				}
			}
		}

		// We're now racing cause this is a vehicle
		auto* character = m_Parent->GetComponent<CharacterComponent>();
		if (character) {
			character->SetIsRacing(true);
		}

		SetPossessable(carEntity->GetObjectID());
		SetPossessableType(ePossessionType::ATTACHED_VISIBLE); // Vehicles are always this

		// make it exist
		EntityManager::Instance()->ConstructEntity(carEntity);
		EntityManager::Instance()->SerializeEntity(m_Parent);
		GameMessages::SendSetJetPackMode(m_Parent, false);

		// Send racing specific GMs
		GameMessages::SendNotifyVehicleOfRacingObject(carEntity->GetObjectID(), m_Parent->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::SendRacingPlayerLoaded(LWOOBJID_EMPTY, m_Parent->GetObjectID(), carEntity->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::SendVehicleUnlockInput(carEntity->GetObjectID(), false, UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::SendTeleport(m_Parent->GetObjectID(), startPosition, startRotation, m_Parent->GetSystemAddress(), true, true);
		GameMessages::SendTeleport(carEntity->GetObjectID(), startPosition, startRotation, m_Parent->GetSystemAddress(), true, true);
	}
	// Make the item background orange 
	GameMessages::SendMarkInventoryItemAsActive(m_Parent->GetObjectID(), true, eUnequippableActiveType::MOUNT, item->GetId(), m_Parent->GetSystemAddress());
	// make sure we update the parent, they may be concerned
	EntityManager::Instance()->SerializeEntity(m_Parent);
	return;
}

void PossessorComponent::Dismount(Item* item) {
	// we are dismounting, so set it to true
	SetIsDismounting(true);
	const auto type = static_cast<eItemType>(item->GetInfo().itemType);
	// Make the item background not orange 
	GameMessages::SendMarkInventoryItemAsActive(m_Parent->GetObjectID(), false, eUnequippableActiveType::MOUNT, item->GetId(), m_Parent->GetSystemAddress());

	if (type == ITEM_TYPE_MOUNT){
		auto* entity = EntityManager::Instance()->GetEntity(GetPossessable());
		if (entity){
			auto* possessableComponent = entity->GetComponent<PossessableComponent>();
			if (possessableComponent) {
				possessableComponent->SetPossessor(LWOOBJID_EMPTY);
			}
			EntityManager::Instance()->SerializeEntity(entity);
		}
	} else if (type == ITEM_TYPE_VEHICLE && m_Parent->GetGMLevel() >= GAME_MASTER_LEVEL_OPERATOR) {
		GameMessages::SendNotifyRacingClient(LWOOBJID_EMPTY, 3, 0, LWOOBJID_EMPTY, u"", m_Parent->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		CharacterComponent* characterComponent;
		if (m_Parent->TryGetComponent(COMPONENT_TYPE_CHARACTER, characterComponent)) {
			characterComponent->SetIsRacing(false);
			auto* entity = EntityManager::Instance()->GetEntity(GetPossessable());
			if(entity) {
				auto* possessableComponent = entity->GetComponent<PossessableComponent>();

				if (possessableComponent) {
					possessableComponent->SetPossessor(LWOOBJID_EMPTY);
				}
				EntityManager::Instance()->SerializeEntity(entity);
			}
		}
	}
	// make sure we update the parent, they may be concerned
	EntityManager::Instance()->SerializeEntity(m_Parent);
	return;
}