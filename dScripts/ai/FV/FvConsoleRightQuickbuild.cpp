#include "FvConsoleRightQuickbuild.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "eTerminateType.h"
#include "eRebuildState.h"

void FvConsoleRightQuickbuild::OnStartup(Entity* self) {
	self->SetVar(u"IAmBuilt", false);
	self->SetVar(u"AmActive", false);
}

void FvConsoleRightQuickbuild::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state == eRebuildState::COMPLETED) {
		self->SetVar(u"IAmBuilt", true);

		const auto objects = Game::entityManager->GetEntitiesInGroup("Facility");

		if (!objects.empty()) {
			objects[0]->NotifyObject(self, "ConsoleRightUp");
		}
	} else if (state == eRebuildState::RESETTING) {
		self->SetVar(u"IAmBuilt", false);
		self->SetVar(u"AmActive", false);

		const auto objects = Game::entityManager->GetEntitiesInGroup("Facility");

		if (!objects.empty()) {
			objects[0]->NotifyObject(self, "ConsoleRightDown");
		}
	}
}

void FvConsoleRightQuickbuild::OnUse(Entity* self, Entity* user) {
	if (self->GetVar<bool>(u"AmActive")) {
		return;
	}

	if (self->GetVar<bool>(u"IAmBuilt")) {
		self->SetVar(u"AmActive", true);

		const auto objects = Game::entityManager->GetEntitiesInGroup("Facility");

		if (!objects.empty()) {
			objects[0]->NotifyObject(self, "ConsoleRightActive");
		}
	}

	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}
