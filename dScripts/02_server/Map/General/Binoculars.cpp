#include "Binoculars.h"
#include "Character.h"
#include "GameMessages.h"
#include "Game.h"
#include "dServer.h"

void Binoculars::OnUse(Entity* self, Entity* user) {
	const auto number = self->GetVarAsString(u"number");

	const int32_t flag = GeneralUtils::TryParse(std::to_string(Game::server->GetZoneID()).substr(0, 2) + number, 0);
	GameMessages::GetFlag flagMsg;
	flagMsg.target = user->GetObjectID();
	flagMsg.flagID = flag;
	flagMsg.Send();
	if (!flagMsg.flag) {
		auto* const character = user->GetCharacter();
		if (character) character->SetPlayerFlag(flag, true);
		GameMessages::SendFireEventClientSide(self->GetObjectID(), user->GetSystemAddress(), u"achieve", LWOOBJID_EMPTY, 0, -1, LWOOBJID_EMPTY);
	}
}
