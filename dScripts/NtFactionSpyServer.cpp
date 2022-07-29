#include "NtFactionSpyServer.h"
#include "Character.h"
#include "ProximityMonitorComponent.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "MissionComponent.h"

void NtFactionSpyServer::OnStartup(Entity* self) {
	SetVariables(self);

	// Set the proximity to sense later
	auto* proximityMonitor = self->GetComponent<ProximityMonitorComponent>();
	if (proximityMonitor == nullptr) {
		proximityMonitor = new ProximityMonitorComponent(self, -1, -1);
		self->AddComponent(COMPONENT_TYPE_PROXIMITY_MONITOR, proximityMonitor);
	}

	proximityMonitor->SetProximityRadius(self->GetVar<float_t>(m_SpyProximityVariable), m_ProximityName);
}

void NtFactionSpyServer::SetVariables(Entity* self) {
	self->SetVar<float_t>(m_SpyProximityVariable, 0.0f);
	self->SetVar<SpyData>(m_SpyDataVariable, {});
	self->SetVar<std::vector<SpyDialogue>>(m_SpyDialogueTableVariable, {});

	// If there's an alternating conversation, indices should be provided using the conversationID variables
	self->SetVar<std::vector<LWOOBJID>>(m_SpyCinematicObjectsVariable, { self->GetObjectID() });
}

void NtFactionSpyServer::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name == m_ProximityName && status == "ENTER" && IsSpy(self, entering)) {
		auto cinematic = self->GetVar<std::u16string>(m_SpyCinematicVariable);
		if (!cinematic.empty()) {

			// Save the root of this cinematic so we can identify updates later
			auto cinematicSplit = GeneralUtils::SplitString(cinematic, u'_');
			if (!cinematicSplit.empty()) {
				self->SetVar<std::u16string>(m_CinematicRootVariable, cinematicSplit.at(0));
			}

			GameMessages::SendPlayCinematic(entering->GetObjectID(), cinematic, entering->GetSystemAddress(),
				true, true, true);
		}
	}
}

bool NtFactionSpyServer::IsSpy(Entity* self, Entity* possibleSpy) {
	auto spyData = self->GetVar<SpyData>(m_SpyDataVariable);
	if (!spyData.missionID || !spyData.flagID || !spyData.itemID)
		return false;

	auto* missionComponent = possibleSpy->GetComponent<MissionComponent>();
	auto* inventoryComponent = possibleSpy->GetComponent<InventoryComponent>();
	auto* character = possibleSpy->GetCharacter();

	// A player is a spy if they have the spy mission, have the spy equipment equipped and don't have the spy flag set yet
	return missionComponent != nullptr && missionComponent->GetMissionState(spyData.missionID) == MissionState::MISSION_STATE_ACTIVE
		&& inventoryComponent != nullptr && inventoryComponent->IsEquipped(spyData.itemID)
		&& character != nullptr && !character->GetPlayerFlag(spyData.flagID);
}

void NtFactionSpyServer::OnCinematicUpdate(Entity* self, Entity* sender, eCinematicEvent event,
	const std::u16string& pathName, float_t pathTime, float_t totalTime,
	int32_t waypoint) {

	const auto& cinematicRoot = self->GetVar<std::u16string>(m_CinematicRootVariable);
	auto pathNameCopy = std::u16string(pathName); // Mutable copy
	auto pathSplit = GeneralUtils::SplitString(pathNameCopy, u'_');

	// Make sure we have a path of type <root>_<index>
	if (pathSplit.size() >= 2) {
		auto pathRoot = pathSplit.at(0);
		auto pathIndex = std::stoi(GeneralUtils::UTF16ToWTF8(pathSplit.at(1))) - 1;
		const auto& dialogueTable = self->GetVar<std::vector<SpyDialogue>>(m_SpyDialogueTableVariable);

		// Make sure we're listening to the root we're interested in
		if (pathRoot == cinematicRoot) {
			if (event == STARTED && pathIndex >= 0 && pathIndex < dialogueTable.size()) {

				// If the cinematic started, show part of the conversation
				GameMessages::SendNotifyClientObject(self->GetObjectID(), m_SpyDialogueNotification, 0,
					0, ParamObjectForConversationID(self, dialogueTable.at(pathIndex).conversationID),
					dialogueTable.at(pathIndex).token, sender->GetSystemAddress());

			} else if (event == ENDED && pathIndex >= dialogueTable.size() - 1) {
				auto spyData = self->GetVar<SpyData>(m_SpyDataVariable);
				auto* character = sender->GetCharacter();
				if (character != nullptr) {
					character->SetPlayerFlag(spyData.flagID, true);
				}
			}
		}
	}
}

LWOOBJID NtFactionSpyServer::ParamObjectForConversationID(Entity* self, uint32_t conversationID) {
	auto paramObjects = self->GetVar<std::vector<LWOOBJID>>(m_SpyCinematicObjectsVariable);
	auto index = conversationID >= paramObjects.size() ? paramObjects.size() - 1 : conversationID;
	return paramObjects.at(index);
}
