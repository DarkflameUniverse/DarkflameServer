#include "PetMessages.h"

#include "BitStreamUtils.h"
#include "Entity.h"
#include "PetComponent.h"

namespace {
	bool ReadBricks(RakNet::BitStream& bitStream, std::vector<Brick>& bricks) {
		uint32_t count{};
		VALIDATE_READ(bitStream.Read(count));
		if (count > MAX_MESSAGE_LENGTH) return false; // Prevent DoS via unbounded brick count
		if (static_cast<uint64_t>(count) * 64 > bitStream.GetNumberOfUnreadBits()) return false;
		bricks.resize(count);
		for (auto& brick : bricks) {
			VALIDATE_READ(bitStream.Read(brick.designerID));
			VALIDATE_READ(bitStream.Read(brick.materialID));
		}
		return true;
	}

	void WriteBricks(RakNet::BitStream& bitStream, const std::vector<Brick>& bricks) {
		bitStream.Write<uint32_t>(bricks.size());
		for (const auto& brick : bricks) {
			bitStream.Write(brick.designerID);
			bitStream.Write(brick.materialID);
		}
	}
}

namespace GameMessages {
	void NotifyPetTamingMinigame::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(PetID);
		bitStream.Write(PlayerTamingID);
		bitStream.Write(bForceTeleport);
		bitStream.Write(notifyType);
		bitStream.Write(petsDestPos);
		bitStream.Write(telePos);
		BitStreamUtils::WriteOptional(bitStream, teleRot, QuatUtils::IDENTITY);
	}

	bool NotifyPetTamingMinigame::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(PetID));
		VALIDATE_READ(bitStream.Read(PlayerTamingID));
		VALIDATE_READ(bitStream.Read(bForceTeleport));
		VALIDATE_READ(bitStream.Read(notifyType));
		VALIDATE_READ(bitStream.Read(petsDestPos));
		VALIDATE_READ(bitStream.Read(telePos));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, teleRot, QuatUtils::IDENTITY));
		return true;
	}

	void ClientExitTamingMinigame::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bVoluntaryExit);
	}

	bool ClientExitTamingMinigame::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bVoluntaryExit));
		return true;
	}

	void ClientExitTamingMinigame::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* petComponent = PetComponent::GetTamingPet(entity.GetObjectID());
		if (petComponent == nullptr) return;
		petComponent->ClientExitTamingMinigame(bVoluntaryExit);
	}

	void StartServerPetMinigameTimer::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* petComponent = PetComponent::GetTamingPet(entity.GetObjectID());
		if (petComponent == nullptr) return;
		petComponent->StartTimer();
	}

	void NotifyPetTamingPuzzleSelected::Serialize(RakNet::BitStream& bitStream) const {
		WriteBricks(bitStream, bricks);
	}

	bool NotifyPetTamingPuzzleSelected::Deserialize(RakNet::BitStream& bitStream) {
		return ReadBricks(bitStream, bricks);
	}

	void PetTamingTryBuild::Serialize(RakNet::BitStream& bitStream) const {
		WriteBricks(bitStream, bricks);
		bitStream.Write(clientFailed);
	}

	bool PetTamingTryBuild::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadBricks(bitStream, bricks));
		VALIDATE_READ(bitStream.Read(clientFailed));
		return true;
	}

	void PetTamingTryBuild::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* petComponent = PetComponent::GetTamingPet(entity.GetObjectID());
		if (petComponent == nullptr) return;
		petComponent->TryBuild(bricks.size(), clientFailed);
	}

	void PetTamingTryBuildResult::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bSuccess);
		BitStreamUtils::WriteOptional(bitStream, iNumCorrect, 0);
	}

	bool PetTamingTryBuildResult::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bSuccess));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iNumCorrect, 0));
		return true;
	}

	void NotifyTamingBuildSuccess::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(buildPosition);
	}

	bool NotifyTamingBuildSuccess::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(buildPosition));
		return true;
	}

	void NotifyTamingBuildSuccess::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* petComponent = PetComponent::GetTamingPet(entity.GetObjectID());
		if (petComponent == nullptr) return;
		petComponent->NotifyTamingBuildSuccess(buildPosition);
	}

	void PetResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(ObjIDPet);
		bitStream.Write(iPetCommandType);
		bitStream.Write(iResponse);
		bitStream.Write(iTypeID);
	}

	bool PetResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(ObjIDPet));
		VALIDATE_READ(bitStream.Read(iPetCommandType));
		VALIDATE_READ(bitStream.Read(iResponse));
		VALIDATE_READ(bitStream.Read(iTypeID));
		return true;
	}

	void AddPetToPlayer::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(iElementalType);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, name);
		bitStream.Write(petDBID);
		bitStream.Write(petLOT);
	}

	bool AddPetToPlayer::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(iElementalType));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, name));
		VALIDATE_READ(bitStream.Read(petDBID));
		VALIDATE_READ(bitStream.Read(petLOT));
		return true;
	}

	void RegisterPetID::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(objID);
	}

	bool RegisterPetID::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(objID));
		return true;
	}

	void RegisterPetDBID::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(petDBID);
	}

	bool RegisterPetDBID::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(petDBID));
		return true;
	}

	void ShowPetActionButton::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(ButtonLabel);
		bitStream.Write(bShow);
	}

	bool ShowPetActionButton::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(ButtonLabel));
		VALIDATE_READ(bitStream.Read(bShow));
		return true;
	}

	void BouncerActiveStatus::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bActive);
	}

	bool BouncerActiveStatus::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bActive));
		return true;
	}

	void RequestSetPetName::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, name);
	}

	bool RequestSetPetName::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, name));
		return true;
	}

	void RequestSetPetName::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* petComponent = PetComponent::GetTamingPet(entity.GetObjectID());

		if (petComponent == nullptr) {
			petComponent = PetComponent::GetActivePet(entity.GetObjectID());

			if (petComponent == nullptr) {
				return;
			}
		}

		petComponent->RequestSetPetName(name);
	}

	void SetPetName::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, name);
		BitStreamUtils::WriteOptional(bitStream, petDBID, LWOOBJID_EMPTY);
	}

	bool SetPetName::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, name));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, petDBID, LWOOBJID_EMPTY));
		return true;
	}

	void SetPetNameModerated::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, PetDBID, LWOOBJID_EMPTY);
		bitStream.Write(nModerationStatus);
	}

	bool SetPetNameModerated::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, PetDBID, LWOOBJID_EMPTY));
		VALIDATE_READ(bitStream.Read(nModerationStatus));
		return true;
	}

	void PetNameChanged::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(moderationStatus);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, name);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, ownerName);
	}

	bool PetNameChanged::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(moderationStatus));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, name));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, ownerName));
		return true;
	}

	void CommandPet::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(GenericPosInfo);
		bitStream.Write(ObjIDSource);
		bitStream.Write(iPetCommandType);
		bitStream.Write(iTypeID);
		bitStream.Write(overrideObey);
	}

	bool CommandPet::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(GenericPosInfo));
		VALIDATE_READ(bitStream.Read(ObjIDSource));
		VALIDATE_READ(bitStream.Read(iPetCommandType));
		VALIDATE_READ(bitStream.Read(iTypeID));
		VALIDATE_READ(bitStream.Read(overrideObey));
		return true;
	}

	void CommandPet::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* petComponent = entity.GetComponent<PetComponent>();
		if (petComponent == nullptr) return;
		petComponent->Command(GenericPosInfo, ObjIDSource, iPetCommandType, iTypeID, overrideObey);
	}

	void DespawnPet::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bDeletePet);
	}

	bool DespawnPet::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bDeletePet));
		return true;
	}

	void DespawnPet::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* petComponent = PetComponent::GetActivePet(entity.GetObjectID());
		if (petComponent == nullptr) return;

		if (bDeletePet) {
			petComponent->Release();
		} else {
			petComponent->Deactivate();
		}
	}
}
