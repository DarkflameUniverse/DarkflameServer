#include "TriggerGas.h"
#include "InventoryComponent.h"
#include "SkillComponent.h"
#include "Entity.h"
#include "dLogger.h"


void TriggerGas::OnStartup(Entity* self) {
	self->AddTimer(this->m_TimerName, this->m_Time);
}

void TriggerGas::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target->IsPlayer()) return;
	auto players = self->GetVar<std::vector<Entity*>>(u"players");
	players.push_back(target);
	self->SetVar(u"players", players);
}

void TriggerGas::OnOffCollisionPhantom(Entity* self, Entity* target) {
	auto players = self->GetVar<std::vector<Entity*>>(u"players");
	if (!target->IsPlayer() || players.empty()) return;
	auto position = std::find(players.begin(), players.end(), target);
	if (position != players.end()) players.erase(position);
	self->SetVar(u"players", players);
}

void TriggerGas::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName != this->m_TimerName) return;
	auto players = self->GetVar<std::vector<Entity*>>(u"players");
	for (auto player : players) {
		if (player->GetIsDead() || !player){
			auto position = std::find(players.begin(), players.end(), player);
			if (position != players.end()) players.erase(position);
			continue;
		}
		auto inventoryComponent = player->GetComponent<InventoryComponent>();
		if (inventoryComponent) {
			if (!inventoryComponent->IsEquipped(this->m_MaelstromHelmet)) {
				auto* skillComponent = self->GetComponent<SkillComponent>();
				if (skillComponent) {
					skillComponent->CastSkill(this->m_FogDamageSkill, player->GetObjectID());
				}
			}
		}
	}
	self->SetVar(u"players", players);
	self->AddTimer(this->m_TimerName, this->m_Time);
}

