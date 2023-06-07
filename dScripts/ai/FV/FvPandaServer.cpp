#include "FvPandaServer.h"
#include "PetComponent.h"
#include "Character.h"
#include "ePetTamingNotifyType.h"

void FvPandaServer::OnStartup(Entity* self) {
	const auto petComponent = self->GetComponent<PetComponent>();
	if (petComponent != nullptr && petComponent->GetOwner() == nullptr) {
		self->SetNetworkVar<std::string>(u"pandatamer", std::to_string(self->GetVar<LWOOBJID>(u"tamer")));
		self->AddTimer("killSelf", 45);
	}
}

void FvPandaServer::OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, ePetTamingNotifyType type) {
	if (type == ePetTamingNotifyType::BEGIN) {
		self->CancelAllTimers();
	} else if (type == ePetTamingNotifyType::QUIT || type == ePetTamingNotifyType::FAILED) {
		self->Smash();
	} else if (type == ePetTamingNotifyType::SUCCESS) {
		// TODO: Remove from groups

		auto* character = tamer->GetCharacter();
		if (character != nullptr) {
			character->SetPlayerFlag(82, true);
		}
	}
}

void FvPandaServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "killSelf") {
		const auto petComponent = self->GetComponent<PetComponent>();
		if (petComponent != nullptr && petComponent->GetOwner() == nullptr) {
			self->Smash(self->GetObjectID(), eKillType::SILENT);
		}
	}
}
