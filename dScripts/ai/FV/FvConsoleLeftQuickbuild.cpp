#include "FvConsoleLeftQuickbuild.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "eTerminateType.h"
#include "eRebuildState.h"
#include "Entity.h"

void FvConsoleLeftQuickbuild::OnStartup(Entity* self) {
	self->SetVar(u"IAmBuilt", false);
	self->SetVar(u"AmActive", false);
}

void FvConsoleLeftQuickbuild::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state == eRebuildState::COMPLETED) {
		self->SetVar(u"IAmBuilt", true);

		const auto objects = EntityManager::Instance()->GetEntitiesInGroup("Facility");

		if (!objects.empty()) {
			objects[0]->NotifyObject(self, u"ConsoleLeftUp");
		}
	} else if (state == eRebuildState::RESETTING) {
		self->SetVar(u"IAmBuilt", false);
		self->SetVar(u"AmActive", false);

		const auto objects = EntityManager::Instance()->GetEntitiesInGroup("Facility");

		if (!objects.empty()) {
			objects[0]->NotifyObject(self, u"ConsoleLeftDown");
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
			objects[0]->NotifyObject(self, u"ConsoleLeftActive");
		}
	}

	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}
