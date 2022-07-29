#include "Binoculars.h"
#include "Character.h"
#include "GameMessages.h"
#include "Game.h"
#include "dServer.h"

void Binoculars::OnUse(Entity* self, Entity* user) {
	const auto number = self->GetVarAsString(u"number");

	int32_t flag = std::stoi(std::to_string(Game::server->GetZoneID()).substr(0, 2) + number);
	if (user->GetCharacter()->GetPlayerFlag(flag) == false) {
		user->GetCharacter()->SetPlayerFlag(flag, true);
		GameMessages::SendFireEventClientSide(self->GetObjectID(), user->GetSystemAddress(), u"achieve", LWOOBJID_EMPTY, 0, -1, LWOOBJID_EMPTY);
	}
}
