#include "PossessorComponent.h"
#include "PossessableComponent.h"
#include "CharacterComponent.h"
#include "HavokVehiclePhysicsComponent.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "RacingMessages.h"
#include "eUnequippableActiveType.h"
#include "InventoryMessages.h"
#include "MovementMessages.h"
#include "ControllablePhysicsComponent.h"
#include "SkillMessages.h"

PossessorComponent::PossessorComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	m_Possessable = LWOOBJID_EMPTY;
}

PossessorComponent::~PossessorComponent() {
	if (m_Possessable != LWOOBJID_EMPTY) {
		auto* mount = Game::entityManager->GetEntity(m_Possessable);
		if (mount) {
			auto* possessable = mount->GetComponent<PossessableComponent>();
			if (possessable) {
				if (possessable->GetIsItemSpawned()) {
					GameMessages::MarkInventoryItemAsActive markActive;
					markActive.target = m_Parent->GetObjectID();
					markActive.bActive = false;
					markActive.iType = eUnequippableActiveType::MOUNT;
					markActive.itemID = GetMountItemID();
					markActive.Send(m_Parent->GetSystemAddress());
				}
				possessable->Dismount();
			}
		}
	}
}

void PossessorComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(m_DirtyPossesor || bIsInitialUpdate);
	if (m_DirtyPossesor || bIsInitialUpdate) {
		m_DirtyPossesor = false;
		outBitStream.Write(m_Possessable != LWOOBJID_EMPTY);
		if (m_Possessable != LWOOBJID_EMPTY) {
			outBitStream.Write(m_Possessable);
		}
		outBitStream.Write(m_PossessableType);
	}
}

void PossessorComponent::Mount(Entity* mount) {
	// Don't do anything if we are busy dismounting
	if (GetIsDismounting() || !mount) return;

	auto* possessableComponent = mount->GetComponent<PossessableComponent>();
	if (possessableComponent) {
		possessableComponent->SetPossessor(m_Parent->GetObjectID());
		SetPossessable(mount->GetObjectID());
		SetPossessableType(possessableComponent->GetPossessionType());
		if (possessableComponent->GetSkillSet() != 0) {
			GameMessages::UseSkillSet useSkillSet;
			useSkillSet.target = m_Parent->GetObjectID();
			useSkillSet.possessedId = mount->GetObjectID();
			useSkillSet.setId = possessableComponent->GetSkillSet();
			useSkillSet.Send(m_Parent->GetSystemAddress());
		}
	}

	// GM's to send
	GameMessages::SetJetPackMode jetPackMode;
	jetPackMode.target = m_Parent->GetObjectID();
	jetPackMode.bUse = false;
	jetPackMode.Send(UNASSIGNED_SYSTEM_ADDRESS);
	if (mount->GetComponent<HavokVehiclePhysicsComponent>()) {
		auto characterComponent = m_Parent->GetComponent<CharacterComponent>();
		if (characterComponent) characterComponent->SetIsRacing(true);
		GameMessages::VehicleUnlockInput unlockInput;
		unlockInput.target = mount->GetObjectID();
		unlockInput.bLockWheels = false;
		unlockInput.Send(m_Parent->GetSystemAddress());
	}

	Game::entityManager->SerializeEntity(m_Parent);
	Game::entityManager->SerializeEntity(mount);
}

void PossessorComponent::Dismount(Entity* mount, bool forceDismount) {
	// Don't do anything if we are busy dismounting
	if (GetIsDismounting() || !mount) return;
	SetIsDismounting(true);

	if (mount) {
		auto* possessableComponent = mount->GetComponent<PossessableComponent>();
		if (possessableComponent) {
			possessableComponent->SetPossessor(LWOOBJID_EMPTY);
			if (forceDismount) possessableComponent->ForceDepossess();
			if (possessableComponent->GetSkillSet() != 0) {
				GameMessages::UseSkillSet useSkillSet;
				useSkillSet.target = m_Parent->GetObjectID();
				useSkillSet.possessedId = mount->GetObjectID();
				useSkillSet.setId = possessableComponent->GetSkillSet();
				useSkillSet.bRemove = true;
				useSkillSet.Send(m_Parent->GetSystemAddress());
			}
		}
		Game::entityManager->SerializeEntity(m_Parent);
		Game::entityManager->SerializeEntity(mount);

		if (mount->GetComponent<HavokVehiclePhysicsComponent>()) {
			auto characterComponent = m_Parent->GetComponent<CharacterComponent>();
			if (characterComponent) characterComponent->SetIsRacing(false);
		}
	}
}

void PossessorComponent::OnDismountComplete(const LWOOBJID mountId) {
	auto* mount = Game::entityManager->GetEntity(mountId);
	// make sure we have the things we need and they aren't null
	if (!mount) return;
	if (!GetIsDismounting()) return;
	SetIsDismounting(false);
	SetPossessable(LWOOBJID_EMPTY);
	SetPossessableType(ePossessionType::NO_POSSESSION);

	// character related things
	auto* character = m_Parent->GetComponent<CharacterComponent>();
	if (character) {
		// If we had an active item turn it off
		if (GetMountItemID() != LWOOBJID_EMPTY) {
			GameMessages::MarkInventoryItemAsActive markActive;
			markActive.target = m_Parent->GetObjectID();
			markActive.bActive = false;
			markActive.iType = eUnequippableActiveType::MOUNT;
			markActive.itemID = GetMountItemID();
			markActive.Send(m_Parent->GetSystemAddress());
		}
		SetMountItemID(LWOOBJID_EMPTY);
	}

	// Set that the controllabel phsyics comp is teleporting
	auto* controllablePhysicsComponent = m_Parent->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComponent) controllablePhysicsComponent->SetIsTeleporting(true);

	// Call dismoint on the possessable comp to let it handle killing the possessable
	auto* possessableComponent = mount->GetComponent<PossessableComponent>();
	if (possessableComponent) possessableComponent->Dismount();

	// Update the entity that was possessing
	Game::entityManager->SerializeEntity(m_Parent);
}
