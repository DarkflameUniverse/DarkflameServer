#include "BaseInteractDropLootServer.h"
#include "Loot.h"
#include "GameMessages.h"

void BaseInteractDropLootServer::OnUse(Entity* self, Entity* user) {
	BaseUse(self, user);
}

void BaseInteractDropLootServer::BaseUse(Entity* self, Entity* user) {
	auto cooldownTime = self->GetVar<float>(u"cooldownTime");
	if (cooldownTime == 0) cooldownTime = 5;

	uint32_t lootMatrix = self->GetVar<int32_t>(u"UseLootMatrix");
	if (lootMatrix == 0) lootMatrix = self->GetVar<int32_t>(u"smashable_loot_matrix");
	if (lootMatrix == 0) lootMatrix = 715;

	auto useSound = self->GetVar<std::string>(u"sound1");

	if (!useSound.empty()) {
		GameMessages::SendPlayNDAudioEmitter(self, user->GetSystemAddress(), useSound);
	}

	self->SetNetworkVar(u"bInUse", true);

	LootGenerator::Instance().DropLoot(user, self, lootMatrix, 0, 0);

	self->AddCallbackTimer(cooldownTime, [this, self]() {
		self->SetNetworkVar(u"bInUse", false);
		});
}
