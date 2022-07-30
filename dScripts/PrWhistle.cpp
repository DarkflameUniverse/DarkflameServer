#include "PrWhistle.h"
#include "Character.h"
#include "Entity.h"

void PrWhistle::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args == "unlockEmote") {
		auto* character = sender->GetCharacter();

		if (character != nullptr) {
			character->UnlockEmote(115);
		}
	}
}
