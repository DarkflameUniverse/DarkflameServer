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

PossessorComponent::~PossessorComponent() {
	auto possessable = EntityManager::Instance()->GetEntity(m_Possessable);
	if (!possessable) return;
	auto possessableComponent = possessable->GetComponent<PossessableComponent>();
	possessableComponent->SetPossessor(LWOOBJID_EMPTY);
	possessableComponent->ForceDepossess();
	m_DirtyPossesor = true;
	m_Possessable = LWOOBJID_EMPTY;
	m_PossessableType = ePossessionType::NO_POSSESSION;
	possessable->AddCallbackTimer(1.0f, [possessable]{
			EntityManager::Instance()->DestroyEntity(possessable);
			EntityManager::Instance()->SerializeEntity(possessable);
		}
	);

}

void PossessorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_DirtyPossesor || bIsInitialUpdate);
	if (m_DirtyPossesor || bIsInitialUpdate) {
		m_DirtyPossesor = false;
		outBitStream->Write(m_Possessable != LWOOBJID_EMPTY);
		if (m_Possessable != LWOOBJID_EMPTY) {
			outBitStream->Write(m_Possessable);
		}
		outBitStream->Write(m_PossessableType);
	}
}

void PossessorComponent::Mount(Item* item) {
	auto* current = EntityManager::Instance()->GetEntity(GetPossessable());

	// Need to wait for dismounting to be complete to be able to mount somehting else
	if (GetIsDismounting()) return;

	// Handle
	if (current){
		Dismount(GetPossesableItem());
		return;
	}

	// Need to check again inscase we just dismounted above
	if (GetIsDismounting()) return;

	// bind it if it's BOE
	if (item->GetInfo().isBOE) item->SetBound(true);

	SetPossesableItem(item);
	const auto type = static_cast<eItemType>(item->GetInfo().itemType);

	// Pure Vehicle types are GM only
	if (type == ITEM_TYPE_VEHICLE && !(m_Parent->GetGMLevel() >= GAME_MASTER_LEVEL_OPERATOR)) return;

	// spawn the mount
	auto startPosition = m_Parent->GetPosition();
	auto startRotation = m_Parent->GetRotation();

	CDComponentsRegistryTable* compRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");
	auto vehicle = compRegistryTable->GetByIDAndType(item->GetLot(), COMPONENT_TYPE_VEHICLE_PHYSICS) > 0;
	// if we have a vehicle physics component, we need to do some extra work
	if (vehicle) {
		Game::logger->Log("Possessor", "Mounting a vehicle\n");
		// Get the position and rotation of the player to spawn the vehicle
		// and then invert it
		// otherwise, we'll be upside down, thanks
		auto angles = startRotation.GetEulerAngles();
		angles.x -= PI;
		startRotation = NiQuaternion::FromEulerAngles(angles);

		// We're now racing cause this is a vehicle
		auto* character = m_Parent->GetComponent<CharacterComponent>();
		if (character) {
			character->SetIsRacing(true);
		}
		GameMessages::SendTeleport(m_Parent->GetObjectID(), startPosition, startRotation, m_Parent->GetSystemAddress(), true, true);
	}

	EntityInfo info{};
	info.lot = item->GetLot();
	info.pos = startPosition;
	info.rot = startRotation;
	info.spawnerID = m_Parent->GetObjectID();

	auto* mount = EntityManager::Instance()->CreateEntity(info, nullptr, m_Parent);

	// possess it
	auto* possessable = mount->GetComponent<PossessableComponent>();
	if (possessable) {
		Game::logger->Log("Possessor", "Setting up possessorcomponent\n");
		possessable->SetPossessor(m_Parent->GetObjectID());
		auto animationFlag = static_cast<eAnimationFlags>(item->GetInfo().animationFlag);
		possessable->SetAnimationFlag(animationFlag);
		SetPossessableType(possessable->GetPossessionType());
	}
	SetPossessable(mount->GetObjectID());

	// If we have a modular assembly comp, we need to set up it's assemblyPartLOTs from the item's config
	auto* moduleAssemblyComponent = mount->GetComponent<ModuleAssemblyComponent>();
	if (moduleAssemblyComponent) {
		Game::logger->Log("Possessor", "Setting up moduleAssemblyComponent\n");
		moduleAssemblyComponent->SetSubKey(item->GetSubKey());
		moduleAssemblyComponent->SetUseOptionalParts(false);

		for (auto* config : item->GetConfig()) {
			if (config->GetKey() == u"assemblyPartLOTs") {
				moduleAssemblyComponent->SetAssemblyPartsLOTs(GeneralUtils::ASCIIToUTF16(config->GetValueAsString()));
			}
		}
	}

	// Setup the destroyable stats
	auto *destroyableComponent = mount->GetComponent<DestroyableComponent>();
	if (destroyableComponent) {
		Game::logger->Log("Possessor", "Setting up destroyableComponent\n");
		destroyableComponent->SetIsSmashable(false);
		destroyableComponent->SetIsImmune(false);
	}
	// make it exist
	EntityManager::Instance()->ConstructEntity(mount);
	EntityManager::Instance()->SerializeEntity(m_Parent);
	

	// no flying vehicles
	GameMessages::SendSetJetPackMode(m_Parent, false);

	if (vehicle){
		Game::logger->Log("Possessor", "Send racing specific GMs\n");
		// Send racing specific GMs
		GameMessages::SendNotifyVehicleOfRacingObject(mount->GetObjectID(), m_Parent->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::SendRacingPlayerLoaded(LWOOBJID_EMPTY, m_Parent->GetObjectID(), mount->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::SendVehicleUnlockInput(mount->GetObjectID(), false, UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::SendTeleport(m_Parent->GetObjectID(), startPosition, startRotation, m_Parent->GetSystemAddress(), true, true);
		GameMessages::SendTeleport(mount->GetObjectID(), startPosition, startRotation, m_Parent->GetSystemAddress(), true, true);
	}
	if (!vehicle || type == ITEM_TYPE_MOUNT) {
		Game::logger->Log("Possessor", "Send mount specific GMs\n");
		// only send this on mounting
		GameMessages::SendSetMountInventoryID(m_Parent, mount->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		// stun the player
		GameMessages::SendSetStunned(m_Parent->GetObjectID(), eStunState::PUSH, m_Parent->GetSystemAddress(), LWOOBJID_EMPTY, true, false, true, false, false, false, false, true, true, true, true, true, true, true, true, true);
	}
	// Make the item background orange 
	GameMessages::SendMarkInventoryItemAsActive(m_Parent->GetObjectID(), true, eUnequippableActiveType::MOUNT, item->GetId(), m_Parent->GetSystemAddress());

	EntityManager::Instance()->ConstructEntity(mount);
	EntityManager::Instance()->SerializeEntity(m_Parent);

	return;
}

void PossessorComponent::Dismount(Item* item, bool forceDismount) {
	// Need to wait for dismounting to be complete to be able to mount somehting else
	if (GetIsDismounting()) return;
	
	// we are dismounting, so set it to true
	SetIsDismounting(true);
	const auto type = static_cast<eItemType>(item->GetInfo().itemType);

	// Pure Vehicle types are GM only
	if (type == ITEM_TYPE_VEHICLE && !(m_Parent->GetGMLevel() >= GAME_MASTER_LEVEL_OPERATOR)) return;

	// Make the item background not orange 
	GameMessages::SendMarkInventoryItemAsActive(m_Parent->GetObjectID(), false, eUnequippableActiveType::MOUNT, item->GetId(), m_Parent->GetSystemAddress());

	// Special case for vehicles
	if (type == ITEM_TYPE_VEHICLE) {
		GameMessages::SendNotifyRacingClient(LWOOBJID_EMPTY, 3, 0, LWOOBJID_EMPTY, u"", m_Parent->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		CharacterComponent* characterComponent;
		if (m_Parent->TryGetComponent(COMPONENT_TYPE_CHARACTER, characterComponent)) {
			characterComponent->SetIsRacing(false);
		}
	}

	auto* entity = EntityManager::Instance()->GetEntity(GetPossessable());
	if (entity){
		auto* possessableComponent = entity->GetComponent<PossessableComponent>();
		if (possessableComponent) {
			possessableComponent->SetPossessor(LWOOBJID_EMPTY);
			if (forceDismount) possessableComponent->ForceDepossess();
		}
		EntityManager::Instance()->SerializeEntity(m_Parent);
		EntityManager::Instance()->SerializeEntity(entity);
		
	}
	return;
}