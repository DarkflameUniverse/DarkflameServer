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
	possessable->AddCallbackTimer(5.0f, [possessable]{
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
	// TODO: This doesn't actually update the item BOE status on equip, mounts are weird
	if (item->GetInfo().isBOE) item->SetBound(true);

	SetPossesableItem(item);
	const auto type = static_cast<eItemType>(item->GetInfo().itemType);

	// Pure Vehicle types are GM only
	if (type == ITEM_TYPE_VEHICLE && !(m_Parent->GetGMLevel() >= GAME_MASTER_LEVEL_OPERATOR)) return;
	const auto lot = item->GetLot();

	auto inventory = m_Parent->GetComponent<InventoryComponent>();
	if (inventory) {
	inventory->CheckItemSet(lot);
		// for (auto* set : m_Itemsets)
		// {
		// 	set->OnEquip(lot);
		// }

		inventory->GenerateProxies(item);

		inventory->ApplyBuff(item);
		
		inventory->AddItemSkills(item->GetLot());

	}

	// spawn the mount
	auto startPosition = m_Parent->GetPosition();
	auto startRotation = m_Parent->GetRotation();

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

	EntityInfo info{};
	info.lot = item->GetLot();
	info.pos = startPosition;
	info.rot = startRotation;
	info.spawnerID = m_Parent->GetObjectID();

	auto* mount = EntityManager::Instance()->CreateEntity(info, nullptr, m_Parent);

	// If we have a modular assembly comp, we need to set up it's assemblyPartLOTs from the item's config
	auto* moduleAssemblyComponent = mount->GetComponent<ModuleAssemblyComponent>();
	if (moduleAssemblyComponent) {
		moduleAssemblyComponent->SetSubKey(item->GetSubKey());
		moduleAssemblyComponent->SetUseOptionalParts(false);

		for (auto* config : item->GetConfig()) {
			if (config->GetKey() == u"assemblyPartLOTs") {
				moduleAssemblyComponent->SetAssemblyPartsLOTs(GeneralUtils::ASCIIToUTF16(config->GetValueAsString()));
			}
		}
	}
	auto animationFlag = static_cast<eAnimationFlags>(item->GetInfo().animationFlag);
	Mount(m_Parent, mount, animationFlag);
	
	// Make the item background orange 
	GameMessages::SendMarkInventoryItemAsActive(m_Parent->GetObjectID(), true, eUnequippableActiveType::MOUNT, item->GetId(), m_Parent->GetSystemAddress());
	// update slot
	// TODO: This doesn't actually update the item BOE status on equip, mounts are weird
	GameMessages::SendAddItemToInventoryClientSync(m_Parent, m_Parent->GetSystemAddress(), item, item->GetId(), false, 0);
}


void PossessorComponent::Mount(Entity* possessor, Entity* mount, eAnimationFlags animationFlag) {

	// possess it
	auto* possessable = mount->GetComponent<PossessableComponent>();
	if (possessable) {
		possessable->SetPossessor(m_Parent->GetObjectID());
		possessable->SetAnimationFlag(animationFlag);
		SetPossessableType(possessable->GetPossessionType());
	}
	SetPossessable(mount->GetObjectID());

	// Setup the destroyable stats
	auto *destroyableComponent = mount->GetComponent<DestroyableComponent>();
	if (destroyableComponent) {
		destroyableComponent->SetIsSmashable(false);
		destroyableComponent->SetIsImmune(true);
	}
	// make it exist
	EntityManager::Instance()->ConstructEntity(mount);
	EntityManager::Instance()->SerializeEntity(m_Parent);
	

	// no flying vehicles
	GameMessages::SendSetJetPackMode(m_Parent, false);


	GameMessages::SendNotifyVehicleOfRacingObject(mount->GetObjectID(), m_Parent->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::SendRacingPlayerLoaded(m_Parent->GetObjectID(), m_Parent->GetObjectID(), mount->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::SendVehicleUnlockInput(mount->GetObjectID(), false, UNASSIGNED_SYSTEM_ADDRESS);

	// only send this on mounting
	GameMessages::SendSetMountInventoryID(m_Parent, mount->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
	// stun the player
	GameMessages::SendSetStunned(m_Parent->GetObjectID(), eStunState::PUSH, m_Parent->GetSystemAddress(), LWOOBJID_EMPTY, false, false, false, false, false, false, false, true, true, true, true, true, true, true, true, true);
}

void PossessorComponent::Dismount(Entity* entity, bool forceDismount) {

	if (entity){
		auto* possessableComponent = entity->GetComponent<PossessableComponent>();
		if (possessableComponent) {
			possessableComponent->SetPossessor(LWOOBJID_EMPTY);
			if (forceDismount) possessableComponent->ForceDepossess();
		}
		EntityManager::Instance()->SerializeEntity(m_Parent);
		EntityManager::Instance()->SerializeEntity(entity);
	}

	GameMessages::SendNotifyRacingClient(LWOOBJID_EMPTY, 3, 0, LWOOBJID_EMPTY, u"", m_Parent->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
	CharacterComponent* characterComponent;
	if (m_Parent->TryGetComponent(COMPONENT_TYPE_CHARACTER, characterComponent)) {
		characterComponent->SetIsRacing(false);
	}
	// make sure we don't have wacky controlls
	GameMessages::SendSetPlayerControlScheme(entity, eControlSceme::SCHEME_A);
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
	
	auto* entity = EntityManager::Instance()->GetEntity(GetPossessable());
	Dismount(entity);
}