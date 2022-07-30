#include "FireFirstSkillonStartup.h"
#include "Entity.h"
#include "SkillComponent.h"
#include "CDClientDatabase.h"
#include "CDObjectSkillsTable.h"

void FireFirstSkillonStartup::OnStartup(Entity* self) {
	auto skillComponent = self->GetComponent<SkillComponent>();
	if (!skillComponent) return;

	// Get the skill IDs of this object.
	CDObjectSkillsTable* skillsTable = CDClientManager::Instance()->GetTable<CDObjectSkillsTable>("ObjectSkills");
	std::vector<CDObjectSkills> skills = skillsTable->Query([=](CDObjectSkills entry) {return (entry.objectTemplate == self->GetLOT()); });

	// For each skill, cast it with the associated behavior ID.
	for (auto skill : skills) {
		CDSkillBehaviorTable* skillBehaviorTable = CDClientManager::Instance()->GetTable<CDSkillBehaviorTable>("SkillBehavior");
		CDSkillBehavior behaviorData = skillBehaviorTable->GetSkillByID(skill.skillID);

		// Should parent entity be null, make the originator self.
		const auto target = self->GetParentEntity() ? self->GetParentEntity()->GetObjectID() : self->GetObjectID();
		skillComponent->CalculateBehavior(skill.skillID, behaviorData.behaviorID, LWOOBJID_EMPTY, false, false, target);
	}
}
