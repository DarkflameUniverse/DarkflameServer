#include "FvGuildCreate.h"
#include "GameMessages.h"
#include "Amf3.h"

// Server script for Guild Master NPC in FV area.
// This NPC will react to a user interaction and display
// the guild creation screen.

void FvGuildCreate::OnUse(Entity* self, Entity* user) {
	AMFArrayValue args;
	GameMessages::SendUIMessageServerToSingleClient(user, user->GetSystemAddress(), "ToggleGuildCreate", args);
}
