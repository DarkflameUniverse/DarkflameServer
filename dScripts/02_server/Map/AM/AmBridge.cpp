#include "AmBridge.h"
#include "EntityManager.h"

void AmBridge::OnStartup(Entity* self) {

}

void AmBridge::OnRebuildComplete(Entity* self, Entity* target) {
	const auto consoles = EntityManager::Instance()->GetEntitiesInGroup("Console" + GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"bridge")));

	if (consoles.empty()) {
		return;
	}

	auto* console = consoles[0];

	console->NotifyObject(self, "BridgeBuilt");

	self->AddTimer("SmashBridge", 50);
}

void AmBridge::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName != "SmashBridge") {
		return;
	}

	self->Smash(self->GetObjectID(), VIOLENT);
}
