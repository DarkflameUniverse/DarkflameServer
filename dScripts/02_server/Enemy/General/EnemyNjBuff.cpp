#include "EnemyNjBuff.h"
#include "SkillComponent.h"
#include "MissionComponent.h"
#include "eMissionState.h"
#include "BuffComponent.h"
#include "Loot.h"

void EnemyNjBuff::OnStartup(Entity* self) {
	auto* skillComponent = self->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	skillComponent->CalculateBehavior(1127, 24812, self->GetObjectID(), true);
	
	if (self->GetLOT() == 16810) {
		self->SetVar<int>(u"CurrentHealth", 775);		
	}
}

void EnemyNjBuff::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	
	if (self->GetLOT() == 16810) {
		const auto currentHealth = self->GetVar<int>(u"CurrentHealth");
		
		self->SetVar<int>(u"CurrentHealth", currentHealth - damage);			
	} else {		
		self->SetVar<LWOOBJID>(u"attackerID", attacker->GetObjectID());		
		
		auto* missionComponent = attacker->GetComponent<MissionComponent>();
		if (missionComponent == nullptr || missionComponent->GetMissionState(2106) != eMissionState::COMPLETE){
			self->SetVar<std::string>(u"WeaponElementType", "None");
			return;
		} else {
//			Set buff to corresponding element	
			auto* buffComponent = attacker->GetComponent<BuffComponent>();
			if (buffComponent != nullptr && buffComponent->HasBuff(72)) {
				self->SetVar<std::string>(u"WeaponElementType", "Earth");
			} else if (buffComponent != nullptr && buffComponent->HasBuff(73)) {
				self->SetVar<std::string>(u"WeaponElementType", "Lightning");
			} else if (buffComponent != nullptr && buffComponent->HasBuff(70)) {
				self->SetVar<std::string>(u"WeaponElementType", "Ice");
			} else if (buffComponent != nullptr && buffComponent->HasBuff(71)) {
				self->SetVar<std::string>(u"WeaponElementType", "Fire");
			} else {
				self->SetVar<std::string>(u"WeaponElementType", "None");
				return;
			}
//			Run die callback if not already active		
			if (self->GetVar<int>(u"DieCallbackActive") != 1) {			
				RunDieCallback(self);			
			}
		}	
	}	
}	

void EnemyNjBuff::RunDieCallback(Entity* self) {
	self->SetVar<int>(u"DieCallbackActive", 1);	
	self->AddDieCallback([self, this]() {
		const auto killerID = self->GetVar<LWOOBJID>(u"attackerID");
		auto* killer = Game::entityManager->GetEntity(killerID);	
		auto* missionComponent = killer->GetComponent<MissionComponent>();	
		
		if (!killer->IsPlayer() || missionComponent->GetMissionState(2106) != eMissionState::COMPLETE) {
			return;
		}
	
//		Get correct loot table	
		const auto ElementType = self->GetVar<std::string>(u"WeaponElementType");		
		const auto myLOT = self->GetLOT();		
		for (size_t i = 0; i < enemies.size(); ++i) {
			if (std::find(enemies[i].begin(), enemies[i].end(), myLOT) != enemies[i].end()) {
				auto& lootTable = lootTablesByDifficulty[i];
				if (lootTable.find(ElementType) != lootTable.end()) {
					Loot::DropLoot(killer, self, lootTable[ElementType], 0, 0);
				}
				break;
			}
		}	
		
	});		
}