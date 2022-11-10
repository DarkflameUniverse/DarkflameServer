#include "QbEnemyStunner.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"

void QbEnemyStunner::OnRebuildComplete(Entity* self, Entity* target) {
	auto* destroyable = self->GetComponent<DestroyableComponent>();

	if (destroyable != nullptr) {
		destroyable->SetFaction(115);
	}

	auto skillComponent = self->GetComponent<SkillComponent>();
	if (!skillComponent) return;

	// Get the skill IDs of this object.
	CDObjectSkillsTable* skillsTable = CDClientManager::Instance()->GetTable<CDObjectSkillsTable>("ObjectSkills");
	auto skills = skillsTable->Query([=](CDObjectSkills entry) {return (entry.objectTemplate == self->GetLOT()); });
	std::map<uint32_t, uint32_t> skillBehaviorMap;
	// For each skill, cast it with the associated behavior ID.
	for (auto skill : skills) {
		CDSkillBehaviorTable* skillBehaviorTable = CDClientManager::Instance()->GetTable<CDSkillBehaviorTable>("SkillBehavior");
		CDSkillBehavior behaviorData = skillBehaviorTable->GetSkillByID(skill.skillID);

		skillBehaviorMap.insert(std::make_pair(skill.skillID, behaviorData.behaviorID));
	}

	// If there are no skills found, insert a default skill to use.
	if (skillBehaviorMap.size() == 0) {
		skillBehaviorMap.insert(std::make_pair(499U, 6095U));
	}

	// Start all skills associated with the object next tick
	self->AddTimer("TickTime", 0);

	self->AddTimer("PlayEffect", 20);

	self->SetVar<std::map<uint32_t, uint32_t>>(u"skillBehaviorMap", skillBehaviorMap);
}

void QbEnemyStunner::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "DieTime") {
		self->Smash();

		self->CancelAllTimers();
	} else if (timerName == "PlayEffect") {
		self->SetNetworkVar(u"startEffect", 5.0f, UNASSIGNED_SYSTEM_ADDRESS);

		self->AddTimer("DieTime", 5.0f);
	} else if (timerName == "TickTime") {
		auto* skillComponent = self->GetComponent<SkillComponent>();

		if (skillComponent != nullptr) {
			auto skillBehaviorMap = self->GetVar<std::map<uint32_t, uint32_t>>(u"skillBehaviorMap");
			if (skillBehaviorMap.size() == 0) {
				// Should no skills have been found, default to the mermaid stunner
				skillComponent->CalculateBehavior(499U, 6095U, LWOOBJID_EMPTY);
			} else {
				for (auto pair : skillBehaviorMap) {
					skillComponent->CalculateBehavior(pair.first, pair.second, LWOOBJID_EMPTY);
				}
			}
		}
		self->AddTimer("TickTime", 1);
	}
}
