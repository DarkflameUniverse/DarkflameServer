#include "DLUVanityTeleportingObject.h"
#include "GameMessages.h"
#include "dServer.h"
#include "VanityUtilities.h"
#include "RenderComponent.h"

void DLUVanityTeleportingObject::OnStartup(Entity* self) {
	if (!self->HasVar(u"npcName") || !self->HasVar(u"teleport")) return;
	m_Object = VanityUtilities::GetObject(self->GetVarAsString(u"npcName"));

	if (!m_Object) return;
	if (self->HasVar(u"teleportInterval")) m_TeleportInterval = self->GetVar<float>(u"teleportInterval");

	if (self->GetVar<bool>(u"teleport")) {
		self->AddTimer("setupTeleport", m_TeleportInterval);
	}
}

void DLUVanityTeleportingObject::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "setupTeleport") {
		RenderComponent::PlayAnimation(self, u"interact");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 6478, u"teleportBeam", "teleportBeam");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 6478, u"teleportRings", "teleportRings");

		self->AddTimer("teleport", 2.0f);
		self->AddTimer("stopFX", 2.0f);
	} else if (timerName == "stopFX") {
		GameMessages::SendStopFXEffect(self, true, "teleportBeam");
		GameMessages::SendStopFXEffect(self, true, "teleportRings");
	} else if (timerName == "teleport") {
		std::vector<VanityObjectLocation>& locations = m_Object->m_Locations[Game::server->GetZoneID()];

	selectLocation:
		VanityObjectLocation& newLocation = locations[GeneralUtils::GenerateRandomNumber<size_t>(0, locations.size() - 1)];

		// try to get not the same position, but if we get the same one twice, it's fine
		if (self->GetPosition() == newLocation.m_Position) {
			VanityObjectLocation& newLocation = locations[GeneralUtils::GenerateRandomNumber<size_t>(0, locations.size() - 1)];
		}

		self->SetPosition(newLocation.m_Position);
		self->SetRotation(newLocation.m_Rotation);
		self->SetScale(newLocation.m_Scale);
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 6478, u"teleportBeam", "teleportBeam");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 6478, u"teleportRings", "teleportRings");
		self->AddTimer("stopFX", 2.0f);
		self->AddTimer("setupTeleport", m_TeleportInterval);
	}
}
