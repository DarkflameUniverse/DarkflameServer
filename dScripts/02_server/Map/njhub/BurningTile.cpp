#include "BurningTile.h"
#include "SkillComponent.h"

void BurningTile::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == "PlayerEntered") {
		auto* skillComponent = sender->GetComponent<SkillComponent>();

		if (skillComponent == nullptr) {
			return;
		}

		skillComponent->CalculateBehavior(726, 11723, sender->GetObjectID(), true);
	}
}
