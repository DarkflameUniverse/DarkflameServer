#include "NjGarmadonCelebration.h"

#include "GameMessages.h"
#include "ePlayerFlag.h"

void NjGarmadonCelebration::OnCollisionPhantom(Entity* self, Entity* target) {
	GameMessages::GetFlag getFlag{};
	getFlag.target = target->GetObjectID();
	getFlag.iFlagId = ePlayerFlag::NJ_GARMADON_CINEMATIC_SEEN;

	if (SEND_ENTITY_MSG(getFlag) && !getFlag.bFlag) {
		GameMessages::SetFlag setFlag{};
		setFlag.target = target->GetObjectID();
		setFlag.iFlagId = ePlayerFlag::NJ_GARMADON_CINEMATIC_SEEN;
		setFlag.bFlag = true;
		SEND_ENTITY_MSG(setFlag);

		GameMessages::SendStartCelebrationEffect(target, target->GetSystemAddress(), GarmadonCelebrationID);
	}
}
