#include "NtImagimeterVisibility.h"
#include "GameMessages.h"
#include "ObjectMessages.h"
#include "Entity.h"
#include "Character.h"
#include "ePlayerFlag.h"

void NTImagimeterVisibility::OnQuickBuildComplete(Entity* self, Entity* target) {
	auto* character = target->GetCharacter();
	if (character) character->SetPlayerFlag(ePlayerFlag::NT_PLINTH_REBUILD, true);

	GameMessages::NotifyClientObject(self->GetObjectID(), u"PlinthBuilt", 0, 0, LWOOBJID_EMPTY, "").Send(target->GetSystemAddress());
}
