#include "PropertyDevice.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "MissionComponent.h"
#include "eMissionState.h"

void PropertyDevice::OnStartup(Entity* self) {
	auto* zoneControl = Game::entityManager->GetZoneControlEntity();
	if (zoneControl != nullptr) {
		zoneControl->OnFireEventServerSide(self, "CheckForPropertyOwner");
	}
}

void PropertyDevice::OnRebuildComplete(Entity* self, Entity* target) {
	auto propertyOwnerID = self->GetNetworkVar<std::string>(m_PropertyOwnerVariable);
	if (propertyOwnerID == std::to_string(LWOOBJID_EMPTY))
		return;

	auto* missionComponent = target->GetComponent<MissionComponent>();
	if (missionComponent != nullptr) {
		if (missionComponent->GetMissionState(m_PropertyMissionID) == eMissionState::ACTIVE) {
			GameMessages::SendPlayFXEffect(self->GetObjectID(), 641, u"create", "callhome");
			missionComponent->ForceProgress(m_PropertyMissionID, 1793, self->GetLOT());
		}
	}
}
