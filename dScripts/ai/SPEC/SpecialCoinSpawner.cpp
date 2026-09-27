#include "SpecialCoinSpawner.h"
#include "CharacterComponent.h"
#include "EffectsMessages.h"

void SpecialCoinSpawner::OnStartup(Entity* self) {
	self->SetProximityRadius(1.5f, "powerupEnter");
}

void SpecialCoinSpawner::OnProximityUpdate(Entity* self, Entity* entering, const std::string name, const std::string status) {
	if (name != "powerupEnter" && status != "ENTER") return;
	if (!entering->IsPlayer()) return;
	auto character = entering->GetCharacter();
	if (!character) return;
	GameMessages::PlayFXEffect(self->GetObjectID(), -1, u"pickup", "").Send(UNASSIGNED_SYSTEM_ADDRESS);
	character->SetCoins(character->GetCoins() + this->m_CurrencyDenomination, eLootSourceType::CURRENCY);
	self->Smash(entering->GetObjectID(), eKillType::SILENT);
}
