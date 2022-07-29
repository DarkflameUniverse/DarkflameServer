#include "FvConsoleLeftQuickbuild.h"
#include "EntityManager.h"
#include "GameMessages.h"

void FvConsoleLeftQuickbuild::OnStartup(Entity* self) {
	self->SetVar(u"IAmBuilt", false);
	self->SetVar(u"AmActive", false);
}

void FvConsoleLeftQuickbuild::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state == REBUILD_COMPLETED) {
		self->SetVar(u"IAmBuilt", true);

		const auto objects = EntityManager::Instance()->GetEntitiesInGroup("Facility");

		if (!objects.empty()) {
			objects[0]->NotifyObject(self, "ConsoleLeftUp");
		}
	} else if (state == REBUILD_RESETTING) {
		self->SetVar(u"IAmBuilt", false);
		self->SetVar(u"AmActive", false);

		const auto objects = EntityManager::Instance()->GetEntitiesInGroup("Facility");

		if (!objects.empty()) {
			objects[0]->NotifyObject(self, "ConsoleLeftDown");
		}
	}
}

void FvConsoleLeftQuickbuild::OnUse(Entity* self, Entity* user) {
	if (self->GetVar<bool>(u"AmActive")) {
		return;
	}

	if (self->GetVar<bool>(u"IAmBuilt")) {
		self->SetVar(u"AmActive", true);

		const auto objects = EntityManager::Instance()->GetEntitiesInGroup("Facility");

		if (!objects.empty()) {
			objects[0]->NotifyObject(self, "ConsoleLeftActive");
		}
	}

	GameMessages::SendTerminateInteraction(user->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
}
