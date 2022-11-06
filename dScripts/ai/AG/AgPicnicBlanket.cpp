#include "AgPicnicBlanket.h"
#include "GameMessages.h"

void AgPicnicBlanket::OnUse(Entity* self, Entity* user) {
	GameMessages::SendTerminateInteraction(user->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
	if (self->GetVar<bool>(u"active"))
		return;
	self->SetVar<bool>(u"active", true);

	auto lootTable = std::unordered_map<LOT, int32_t>{ {935, 3} };
	LootGenerator::Instance().DropLoot(user, self, lootTable, 0, 0);

	self->AddCallbackTimer(5.0f, [self]() {
		self->SetVar<bool>(u"active", false);
		});
}
