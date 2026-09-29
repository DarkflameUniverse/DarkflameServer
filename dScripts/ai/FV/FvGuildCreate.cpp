#include "FvGuildCreate.h"

#include "Entity.h"
#include "PlayerMessages.h"

void FvGuildCreate::OnUse(Entity* self, Entity* user) {
	GameMessages::DisplayGuildCreateBox box;
	box.target = user->GetObjectID();
	box.bShow = true;
	box.Send(user->GetSystemAddress());
}
