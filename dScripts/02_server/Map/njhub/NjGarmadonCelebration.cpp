#include "NjGarmadonCelebration.h"
#include "Character.h"
#include "GameMessages.h"
#include "ePlayerFlag.h"

void NjGarmadonCelebration::OnCollisionPhantom(Entity* self, Entity* target) {
	auto* character = target->GetCharacter();

	if (character == nullptr) {
		return;
	}

	if (!character->GetPlayerFlag(ePlayerFlag::NJ_GARMADON_CINEMATIC_SEEN)) {
		character->SetPlayerFlag(ePlayerFlag::NJ_GARMADON_CINEMATIC_SEEN, true);

		GameMessages::SendStartCelebrationEffect(target, target->GetSystemAddress(), GarmadonCelebrationID);
	}
}
