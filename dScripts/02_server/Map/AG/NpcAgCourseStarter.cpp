#include "NpcAgCourseStarter.h"
#include "EntityManager.h"
#include "ScriptedActivityComponent.h"
#include "GameMessages.h"
#include "LeaderboardManager.h"
#include "dServer.h"
#include "eMissionTaskType.h"
#include "eMissionState.h"
#include "MissionComponent.h"
#include <chrono>

void NpcAgCourseStarter::OnStartup(Entity* self) {}

void NpcAgCourseStarter::OnUse(Entity* self, Entity* user) {
	auto* const scriptedActivityComponent = self->GetComponent<ScriptedActivityComponent>();
	if (!scriptedActivityComponent) return;

	const auto selfId = self->GetObjectID();
	const auto userId = user->GetObjectID();
	const auto& userSysAddr = user->GetSystemAddress();

	if (scriptedActivityComponent->GetActivityPlayerData(userId) != nullptr) {
		GameMessages::SendNotifyClientObject(selfId, u"exit", 0, 0, LWOOBJID_EMPTY, "", userSysAddr);
	} else {
		GameMessages::SendNotifyClientObject(selfId, u"start", 0, 0, LWOOBJID_EMPTY, "", userSysAddr);
	}
}

void NpcAgCourseStarter::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	auto* const scriptedActivityComponent = self->GetComponent<ScriptedActivityComponent>();
	if (!scriptedActivityComponent) return;

	const auto selfId = self->GetObjectID();
	const auto senderId = sender->GetObjectID();
	const auto& senderSysAddr = sender->GetSystemAddress();

	if (identifier == u"player_dialog_cancel_course" && button == 1) {
		GameMessages::SendNotifyClientObject(selfId, u"stop_timer", 0, 0, LWOOBJID_EMPTY, "", senderSysAddr);
		GameMessages::SendNotifyClientObject(selfId, u"cancel_timer", 0, 0, LWOOBJID_EMPTY, "", senderSysAddr);

		scriptedActivityComponent->RemoveActivityPlayerData(senderId);

		Game::entityManager->SerializeEntity(self);
	} else if (identifier == u"player_dialog_start_course" && button == 1) {
		GameMessages::SendNotifyClientObject(selfId, u"start_timer", 0, 0, LWOOBJID_EMPTY, "", senderSysAddr);
		GameMessages::SendActivityStart(selfId, senderSysAddr);

		auto* const data = scriptedActivityComponent->AddActivityPlayerData(senderId);
		if (data->values[1] != 0) return;

		const auto raceStartTime = Game::server->GetUptime() + std::chrono::seconds(4); // Offset for starting timer
		const auto fRaceStartTime = std::chrono::duration<float, std::ratio<1>>(raceStartTime).count();
		data->values[1] = fRaceStartTime;

		Game::entityManager->SerializeEntity(self);
	} else if (identifier == u"FootRaceCancel") {
		GameMessages::SendNotifyClientObject(selfId, u"stop_timer", 0, 0, LWOOBJID_EMPTY, "", senderSysAddr);

		if (scriptedActivityComponent->GetActivityPlayerData(senderId) != nullptr) {
			GameMessages::SendNotifyClientObject(selfId, u"exit", 0, 0, LWOOBJID_EMPTY, "", senderSysAddr);
		} else {
			GameMessages::SendNotifyClientObject(selfId, u"start", 0, 0, LWOOBJID_EMPTY, "", senderSysAddr);
		}

		scriptedActivityComponent->RemoveActivityPlayerData(senderId);
	}
}

void NpcAgCourseStarter::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	auto* const scriptedActivityComponent = self->GetComponent<ScriptedActivityComponent>();
	if (!scriptedActivityComponent) return;

	const auto selfId = self->GetObjectID();
	const auto senderId = sender->GetObjectID();
	const auto& senderSysAddr = sender->GetSystemAddress();

	auto* const data = scriptedActivityComponent->GetActivityPlayerData(senderId);
	if (!data) return;

	if (args == "course_cancel") {
		GameMessages::SendNotifyClientObject(selfId, u"cancel_timer", 0, 0,
			LWOOBJID_EMPTY, "", senderSysAddr);
		scriptedActivityComponent->RemoveActivityPlayerData(senderId);
	} else if (args == "course_finish") {
		const auto raceEndTime = Game::server->GetUptime();
		const auto fRaceEndTime = std::chrono::duration<float, std::ratio<1>>(raceEndTime).count();
		const auto raceTimeElapsed = fRaceEndTime - data->values[1];
		data->values[2] = raceTimeElapsed;

		auto* const missionComponent = sender->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->ForceProgressTaskType(1884, 1, 1, false);
			missionComponent->Progress(eMissionTaskType::PERFORM_ACTIVITY, -raceTimeElapsed, selfId,
				"performact_time");
		}

		Game::entityManager->SerializeEntity(self);
		LeaderboardManager::SaveScore(senderId, scriptedActivityComponent->GetActivityID(), raceTimeElapsed);

		GameMessages::SendNotifyClientObject(selfId, u"ToggleLeaderBoard",
			scriptedActivityComponent->GetActivityID(), 0, senderId,
			"", senderSysAddr);
		GameMessages::SendNotifyClientObject(selfId, u"stop_timer", 1, raceTimeElapsed, LWOOBJID_EMPTY, "",
			senderSysAddr);

		scriptedActivityComponent->RemoveActivityPlayerData(senderId);
	}
}
