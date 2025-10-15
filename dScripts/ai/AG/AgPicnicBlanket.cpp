#include "AgPicnicBlanket.h"
#include "Loot.h"
#include "GameMessages.h"
#include "Entity.h"
#include "eTerminateType.h"

void AgPicnicBlanket::OnUse(Entity* self, Entity* user) {
	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
	if (self->GetVar<bool>(u"active"))
		return;
	self->SetVar<bool>(u"active", true);

	GameMessages::GetPosition posMsg{};
	posMsg.target = self->GetObjectID();
	posMsg.Send();

	for (int32_t i = 0; i < 3; i++) {
		GameMessages::DropClientLoot lootMsg{};
		lootMsg.target = user->GetObjectID();
		lootMsg.ownerID = user->GetObjectID();
		lootMsg.sourceID = self->GetObjectID();
		lootMsg.item = 935;
		lootMsg.count = 1;
		lootMsg.spawnPos = posMsg.pos;
		lootMsg.currency = 0;
		Loot::DropItem(*user, lootMsg, true);
	}

	self->AddCallbackTimer(5.0f, [self]() {
		self->SetVar<bool>(u"active", false);
		});
}
