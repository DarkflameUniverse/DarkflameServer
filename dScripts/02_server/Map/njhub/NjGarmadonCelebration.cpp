#include "NjGarmadonCelebration.h"
#include "Character.h"
#include "GameMessages.h"

void NjGarmadonCelebration::OnCollisionPhantom(Entity* self, Entity* target) {
	auto* character = target->GetCharacter();

	if (character == nullptr) {
		return;
	}

	if (!character->GetPlayerFlag(ePlayerFlags::NJ_GARMADON_CINEMATIC_SEEN)) {
		character->SetPlayerFlag(ePlayerFlags::NJ_GARMADON_CINEMATIC_SEEN, true);

		GameMessages::SendStartCelebrationEffect(target, target->GetSystemAddress(), GarmadonCelebrationID);
	}
}
