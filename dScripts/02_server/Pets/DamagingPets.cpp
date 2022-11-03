#include "DamagingPets.h"
#include "PetComponent.h"
#include "DestroyableComponent.h"
#include "BaseCombatAIComponent.h"
#include "RenderComponent.h"

void DamagingPets::OnStartup(Entity* self) {

	// Make the pet hostile or non-hostile based on whether or not it is tamed
	const auto* petComponent = self->GetComponent<PetComponent>();
	if (petComponent != nullptr && petComponent->GetOwner() == nullptr) {
		self->AddTimer("GoEvil", 0.5f);
	}
}

void DamagingPets::OnPlayerLoaded(Entity* self, Entity* player) {

	// Makes it so that new players also see the effect
	self->AddCallbackTimer(2.5f, [self]() {
		if (self != nullptr) {
			const auto* petComponent = self->GetComponent<PetComponent>();
			if (petComponent != nullptr && petComponent->GetOwner() == nullptr && self->GetVar<bool>(u"IsEvil")) {
				auto* renderComponent = self->GetComponent<RenderComponent>();
				if (renderComponent != nullptr) {
					auto counter = 1;
					for (const auto petEffect : GetPetInfo(self).effect) {
						renderComponent->PlayEffect(petEffect, u"create", "FXname" + std::to_string(counter));
						counter++;
					}
				}
			}
		}
		});
}

void DamagingPets::OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, eNotifyType type) {
	switch (type) {
	case NOTIFY_TYPE_SUCCESS:
	case NOTIFY_TYPE_BEGIN:
		self->CancelAllTimers();
		ClearEffects(self);
		break;
	case NOTIFY_TYPE_FAILED:
	case NOTIFY_TYPE_QUIT:
	{
		self->SetNetworkVar<bool>(u"bIAmTamable", false);
		self->AddTimer("GoEvil", 1.0f);
		break;
	}
	default:
		break;
	}
}

void DamagingPets::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	const auto infoForPet = GetPetInfo(self);
	if (infoForPet.skill == message) {

		// Only make pets tamable that aren't tamed yet
		const auto* petComponent = self->GetComponent<PetComponent>();
		if (petComponent != nullptr && petComponent->GetOwner() == nullptr && self->GetVar<bool>(u"IsEvil")) {
			ClearEffects(self);
			self->AddTimer("GoEvil", 30.0f);
			self->SetNetworkVar<bool>(u"bIAmTamable", true);
		}
	}
}

void DamagingPets::OnTimerDone(Entity* self, std::string message) {
	if (message == "GoEvil") {
		MakeUntamable(self);
	}
}

void DamagingPets::MakeUntamable(Entity* self) {
	auto* petComponent = self->GetComponent<PetComponent>();

	// If the pet is currently not being tamed, make it hostile
	if (petComponent != nullptr && petComponent->GetStatus() != 5) {
		self->SetNetworkVar<bool>(u"bIAmTamable", false);
		self->SetVar<bool>(u"IsEvil", true);
		petComponent->SetStatus(1);

		auto* combatAIComponent = self->GetComponent<BaseCombatAIComponent>();
		if (combatAIComponent != nullptr) {
			combatAIComponent->SetDisabled(false);
		}

		// Special faction that can attack the player but the player can't attack
		auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
		if (destroyableComponent != nullptr) {
			destroyableComponent->SetFaction(114);
			destroyableComponent->SetHealth(5);
		}

		auto* renderComponent = self->GetComponent<RenderComponent>();
		if (renderComponent != nullptr) {
			auto counter = 1;
			for (const auto petEffect : GetPetInfo(self).effect) {
				renderComponent->PlayEffect(petEffect, u"create", "FXname" + std::to_string(counter));
				counter++;
			}
		}
	}
}

void DamagingPets::ClearEffects(Entity* self) {
	self->SetVar<bool>(u"IsEvil", false);

	auto* petComponent = self->GetComponent<PetComponent>();
	if (petComponent != nullptr) {
		petComponent->SetStatus(67108866);
	}

	auto* combatAIComponent = self->GetComponent<BaseCombatAIComponent>();
	if (combatAIComponent != nullptr) {
		combatAIComponent->SetDisabled(true);
	}

	auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
	if (destroyableComponent != nullptr) {
		destroyableComponent->SetFaction(99);
	}

	auto* renderComponent = self->GetComponent<RenderComponent>();
	if (renderComponent != nullptr) {
		auto counter = 1;
		for (const auto petEffect : GetPetInfo(self).effect) {
			renderComponent->StopEffect("FXname" + std::to_string(counter));
			counter++;
		}
	}
}

PetInfo DamagingPets::GetPetInfo(Entity* self) {
	const auto infoForPet = petInfo.find(self->GetLOT());
	return infoForPet != petInfo.end() ? infoForPet->second : petInfo.begin()->second;
}

// Does not compile on Win32 with name specifiers
const std::map<LOT, PetInfo> DamagingPets::petInfo = {
		{ 5639, { /*.effect =*/ { 3170, 4058 }, /*.skill =*/ "waterspray"}}, // Red dragon
		{ 5641, { /*.effect =*/ { 3170, 4058 }, /*.skill =*/ "waterspray"}}, // Green dragon
		{ 3261, { /*.effect =*/ { 1490 }, /*.skill =*/ "waterspray"}}, // Skunk
};
