#include "SpawnPetBaseServer.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "PetComponent.h"

void SpawnPetBaseServer::OnStartup(Entity* self) {
	SetVariables(self);
	self->SetVar<std::string>(u"spawnedPets", "");
}

void SpawnPetBaseServer::OnUse(Entity* self, Entity* user) {
	auto possibleSpawners = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(u"petType") + "Spawner");
	if (possibleSpawners.empty())
		return;

	if (!CheckNumberOfPets(self, user))
		return;

	auto* spawner = possibleSpawners.at(0);
	auto petType = GeneralUtils::ASCIIToUTF16(self->GetVar<std::string>(u"petType"));

	EntityInfo info{};
	info.pos = spawner->GetPosition();
	info.rot = spawner->GetRotation();
	info.lot = self->GetVar<LOT>(u"petLOT");
	info.spawnerID = self->GetObjectID();
	info.settings = {
		new LDFData<LWOOBJID>(u"tamer", user->GetObjectID()),
		new LDFData<std::u16string>(u"groupID", petType + (GeneralUtils::to_u16string(user->GetObjectID())) + u";" + petType + u"s"),
		new LDFData<std::u16string>(u"spawnAnim", self->GetVar<std::u16string>(u"spawnAnim")),
		new LDFData<float_t>(u"spawnTimer", 1.0f)
	};

	auto* pet = EntityManager::Instance()->CreateEntity(info);
	EntityManager::Instance()->ConstructEntity(pet);

	self->SetVar<std::string>(u"spawnedPets", self->GetVar<std::string>(u"spawnedPets") + ","
		+ std::to_string(pet->GetObjectID()));

	auto spawnCinematic = self->GetVar<std::u16string>(u"spawnCinematic");
	if (!spawnCinematic.empty()) {
		GameMessages::SendPlayCinematic(user->GetObjectID(), spawnCinematic, UNASSIGNED_SYSTEM_ADDRESS);
	}

	GameMessages::SendTerminateInteraction(user->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
}

bool SpawnPetBaseServer::CheckNumberOfPets(Entity* self, Entity* user) {
	auto petIDString = self->GetVar<std::string>(u"spawnedPets");
	auto petIDs = GeneralUtils::SplitString(petIDString, ',');

	// Check all the pets that were tamed in the process or were smashed
	std::vector<LWOOBJID> petsToKeep{};
	for (const auto& petID : petIDs) {
		if (petID.empty())
			continue;

		const auto* spawnedPet = EntityManager::Instance()->GetEntity(std::stoull(petID));
		if (spawnedPet == nullptr)
			continue;

		const auto* petComponent = spawnedPet->GetComponent<PetComponent>();
		if (petComponent == nullptr || petComponent->GetOwner() != nullptr)
			continue;

		// Each user can only spawn one pet
		if (spawnedPet->GetVar<LWOOBJID>(u"tamer") == user->GetObjectID())
			return false;

		petsToKeep.push_back(spawnedPet->GetObjectID());
	}

	self->SetNetworkVar<bool>(u"TooManyPets", petsToKeep.size() >= self->GetVar<uint32_t>(u"maxPets"));

	std::string newPetIDs;
	for (const auto petID : petsToKeep) {
		newPetIDs += (std::to_string(petID) + ",");
	}
	self->SetVar<std::string>(u"spawnedPets", newPetIDs);

	return petsToKeep.size() < self->GetVar<uint32_t>(u"maxPets");
}
