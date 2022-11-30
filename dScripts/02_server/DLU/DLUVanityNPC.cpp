#include "DLUVanityNPC.h"
#include "GameMessages.h"
#include "dServer.h"
#include "VanityUtilities.h"

void DLUVanityNPC::OnStartup(Entity* self) {
	m_NPC = VanityUtilities::GetNPC("averysumner - Destroyer of Worlds");

	if (m_NPC == nullptr) {
		return;
	}

	if (self->GetVar<bool>(u"teleport")) {
		self->AddTimer("setupTeleport", 15.0f);
	}
}

void DLUVanityNPC::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "setupTeleport") {
		GameMessages::SendPlayAnimation(self, u"interact");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 6478, u"teleportBeam", "teleportBeam");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 6478, u"teleportRings", "teleportRings");

		self->AddTimer("teleport", 2.0f);
		self->AddTimer("stopFX", 2.0f);
	} else if (timerName == "stopFX") {
		GameMessages::SendStopFXEffect(self, true, "teleportBeam");
		GameMessages::SendStopFXEffect(self, true, "teleportRings");
	} else if (timerName == "teleport") {
		std::vector<VanityNPCLocation>& locations = m_NPC->m_Locations[Game::server->GetZoneID()];

	selectLocation:
		VanityNPCLocation& newLocation = locations[GeneralUtils::GenerateRandomNumber<size_t>(0, locations.size() - 1)];

		if (self->GetPosition() == newLocation.m_Position) {
			goto selectLocation; // cry about it
		}

		self->SetPosition(newLocation.m_Position);
		self->SetRotation(newLocation.m_Rotation);
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 6478, u"teleportBeam", "teleportBeam");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 6478, u"teleportRings", "teleportRings");
		self->AddTimer("stopFX", 2.0f);
		self->AddTimer("setupTeleport", 15.0f);
	}
}
