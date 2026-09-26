#include "TreasureChestDragonServer.h"
#include "ScriptedActivityComponent.h"
#include "TeamManager.h"
#include "EntityManager.h"
#include "Loot.h"

void TreasureChestDragonServer::OnStartup(Entity* self) {

}

void TreasureChestDragonServer::OnUse(Entity* self, Entity* user) {
	if (self->GetVar<bool>(u"bUsed")) {
		return;
	}

	self->SetVar<bool>(u"bUsed", true);

	auto* scriptedActivityComponent = self->GetComponent<ScriptedActivityComponent>();

	if (scriptedActivityComponent == nullptr) {
		return;
	}
	
	auto* team = TeamManager::Instance()->GetTeam(user->GetObjectID());
	
	Loot::DropActivityLoot(user, self->GetObjectID(), scriptedActivityComponent->GetActivityID(), team ? team->members.size() : 1);

	self->Smash(self->GetObjectID());
}
