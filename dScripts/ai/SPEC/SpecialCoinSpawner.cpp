#include "SpecialCoinSpawner.h"
#include "CharacterComponent.h"

void SpecialCoinSpawner::OnStartup(Entity* self) {
	self->SetProximityRadius(1.5f, "powerupEnter");
}

void SpecialCoinSpawner::OnProximityUpdate(Entity* self, Entity* entering, const std::string name, const std::string status) {
	if (name != "powerupEnter" && status != "ENTER") return;
	if (!entering->IsPlayer()) return;
	auto character = entering->GetCharacter();
	if (!character) return;
	GameMessages::SendPlayFXEffect(self, -1, u"pickup", "", LWOOBJID_EMPTY, 1, 1, true);
	character->SetCoins(character->GetCoins() + this->m_currencyDenomination, eLootSourceType::LOOT_SOURCE_CURRENCY);
	self->Smash(entering->GetObjectID(), eKillType::SILENT);
}
