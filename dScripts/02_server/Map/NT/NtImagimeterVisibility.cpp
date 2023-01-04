#include "NtImagimeterVisibility.h"
#include "GameMessages.h"
#include "Entity.h"
#include "Character.h"

void NTImagimeterVisibility::OnRebuildComplete(Entity* self, Entity* target) {
	auto* character = target->GetCharacter();
	if (character) character->SetPlayerFlag(ePlayerFlags::NT_PLINTH_REBUILD, true);

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"PlinthBuilt", 0, 0, LWOOBJID_EMPTY, "", target->GetSystemAddress());
}
