#include "NpcAgCourseStarter.h"
#include "EntityManager.h"
#include "ScriptedActivityComponent.h"
#include "GameMessages.h"
#include "ActivityMessages.h"
#include "ObjectMessages.h"
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

	if (scriptedActivityComponent->PlayerHasActivityData(userId)) {
		GameMessages::NotifyClientObject(selfId, u"exit", 0, 0, LWOOBJID_EMPTY, "").Send(userSysAddr);
	} else {
		GameMessages::NotifyClientObject(selfId, u"start", 0, 0, LWOOBJID_EMPTY, "").Send(userSysAddr);
	}
}

void NpcAgCourseStarter::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	auto* const scriptedActivityComponent = self->GetComponent<ScriptedActivityComponent>();
	if (!scriptedActivityComponent) return;

	const auto selfId = self->GetObjectID();
	const auto senderId = sender->GetObjectID();
	const auto& senderSysAddr = sender->GetSystemAddress();

	if (identifier == u"player_dialog_cancel_course" && button == 1) {
		GameMessages::NotifyClientObject(selfId, u"stop_timer", 0, 0, LWOOBJID_EMPTY, "").Send(senderSysAddr);
		GameMessages::NotifyClientObject(selfId, u"cancel_timer", 0, 0, LWOOBJID_EMPTY, "").Send(senderSysAddr);

		scriptedActivityComponent->RemoveActivityPlayerData(senderId);

		Game::entityManager->SerializeEntity(self);
	} else if (identifier == u"player_dialog_start_course" && button == 1) {
		GameMessages::NotifyClientObject(selfId, u"start_timer", 0, 0, LWOOBJID_EMPTY, "").Send(senderSysAddr);
		GameMessages::ActivityStart activityStart;
		activityStart.target = selfId;
		activityStart.Send(senderSysAddr);

		const auto score = scriptedActivityComponent->GetActivityValue(senderId, 1);
		if (score != 0 && score != -1.0f) return;

		const auto raceStartTime = Game::server->GetUptime() + std::chrono::seconds(4); // Offset for starting timer
		const auto fRaceStartTime = std::chrono::duration<float, std::ratio<1>>(raceStartTime).count();
		scriptedActivityComponent->SetActivityValue(senderId, 1, fRaceStartTime);

		Game::entityManager->SerializeEntity(self);
	} else if (identifier == u"FootRaceCancel") {
		GameMessages::NotifyClientObject(selfId, u"stop_timer", 0, 0, LWOOBJID_EMPTY, "").Send(senderSysAddr);

		if (scriptedActivityComponent->PlayerHasActivityData(senderId)) {
			GameMessages::NotifyClientObject(selfId, u"exit", 0, 0, LWOOBJID_EMPTY, "").Send(senderSysAddr);
		} else {
			GameMessages::NotifyClientObject(selfId, u"start", 0, 0, LWOOBJID_EMPTY, "").Send(senderSysAddr);
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

	if (!scriptedActivityComponent->PlayerHasActivityData(senderId)) return;

	if (args == "course_cancel") {
		GameMessages::NotifyClientObject(selfId, u"cancel_timer", 0, 0, LWOOBJID_EMPTY, "").Send(senderSysAddr);
		scriptedActivityComponent->RemoveActivityPlayerData(senderId);
	} else if (args == "course_finish") {
		const auto raceEndTime = Game::server->GetUptime();
		const auto fRaceEndTime = std::chrono::duration<float, std::ratio<1>>(raceEndTime).count();
		const float startTime = scriptedActivityComponent->GetActivityValue(senderId, 1);
		if (startTime == 0 || startTime == -1.0f) return;

		const auto raceTimeElapsed = fRaceEndTime - startTime;
		scriptedActivityComponent->SetActivityValue(senderId, 2, raceTimeElapsed);

		auto* const missionComponent = sender->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->ForceProgressTaskType(1884, 1, 1, false);
			missionComponent->Progress(eMissionTaskType::PERFORM_ACTIVITY, -raceTimeElapsed, selfId,
				"performact_time");
		}

		Game::entityManager->SerializeEntity(self);
		LeaderboardManager::SaveScore(senderId, scriptedActivityComponent->GetActivityID(), raceTimeElapsed);

		GameMessages::NotifyClientObject(selfId, u"ToggleLeaderBoard", scriptedActivityComponent->GetActivityID(), 0, senderId, "").Send(senderSysAddr);
		GameMessages::NotifyClientObject(selfId, u"stop_timer", 1, raceTimeElapsed, LWOOBJID_EMPTY, "").Send(senderSysAddr);

		scriptedActivityComponent->RemoveActivityPlayerData(senderId);
	}
}
