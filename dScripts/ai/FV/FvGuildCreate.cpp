#include "FvGuildCreate.h"
#include "GameMessages.h"

// Server script for Guild Master NPC in FV area.
// This NPC will react to a user interaction and display
// the guild creation screen.

void FvGuildCreate::OnUse(Entity* self, Entity* user) {
    AMFStringValue* value = new AMFStringValue();
    value->SetStringValue("ToggleGuildCreate");
    AMFArrayValue args;
    GameMessages::SendUIMessageServerToSingleClient(user, user->GetSystemAddress(), value->GetStringValue(), &args);
    delete value;
}
