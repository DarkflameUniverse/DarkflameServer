#include "GfParrotCrash.h"
#include "SkillComponent.h"
#include "Entity.h"
#include "dLogger.h"

void GfParrotCrash::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	auto* skillComponent = self->GetComponent<SkillComponent>();
	if (args == "Slow") {
		skillComponent->CalculateBehavior(m_SlowSkillID, m_SlowBehaviorID, sender->GetObjectID());
	} else if (args == "Unslow") {
		skillComponent->CalculateBehavior(m_UnslowSkillID, m_UnslowBehaviorID, sender->GetObjectID());
	}
}
