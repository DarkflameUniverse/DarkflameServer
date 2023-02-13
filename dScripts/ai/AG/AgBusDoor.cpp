#include "AgBusDoor.h"
#include "Entity.h"
#include "GameMessages.h"
#include "ProximityMonitorComponent.h"

void AgBusDoor::OnStartup(Entity* self) {
	m_Counter = 0;
	m_OuterCounter = 0;
	self->SetProximityRadius(75, "busDoor");
	self->SetProximityRadius(85, "busDoorOuter");
}

void AgBusDoor::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name != "busDoor" && name != "busDoorOuter") return;

	// Make sure only humans are taken into account
	if (!entering->GetCharacter()) return;

	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();

	if (proximityMonitorComponent == nullptr) return;

	m_Counter = 0;
	m_OuterCounter = 0;

	for (const auto& pair : proximityMonitorComponent->GetProximityObjects("busDoor")) {
		auto* entity = EntityManager::Instance()->GetEntity(pair.first);
		if (entity != nullptr && entity->IsPlayer()) m_Counter++;
	}

	for (const auto& pair : proximityMonitorComponent->GetProximityObjects("busDoorOuter")) {
		auto* entity = EntityManager::Instance()->GetEntity(pair.first);
		if (entity != nullptr && entity->IsPlayer()) m_OuterCounter++;
	}

	if (status == "ENTER") {
		// move up when a player is inside both radii
		if (m_Counter > 0) {
			MoveDoor(self, true);
		}
	} else if (status == "LEAVE") {
		// move down when no players are inside either radii
		if (m_Counter <= 0) {
			MoveDoor(self, false);
		}
	}
}

void AgBusDoor::MoveDoor(Entity* self, bool bOpen) {
	if (bOpen) {
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0);
	} else {
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1);
		self->AddTimer("dustTimer", 2.0f);
	}

	//This is currently commented out because it might be the reason that people's audio is cutting out.
	GameMessages::SendPlayNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, "{9a24f1fa-3177-4745-a2df-fbd996d6e1e3}");
}

void AgBusDoor::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "dustTimer") {
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 642, u"create", "busDust", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
	}
}
