#include "NpcAgCourseStarter.h"
#include "EntityManager.h"
#include "ScriptedActivityComponent.h"
#include "GameMessages.h"
#include "LeaderboardManager.h"
#include "eMissionTaskType.h"
#include "eMissionState.h"
#include "MissionComponent.h"
#include <ctime>
#include <chrono>
#include "dServer.h"

void NpcAgCourseStarter::OnStartup(Entity* self) {

}

void NpcAgCourseStarter::OnUse(Entity* self, Entity* user) {
	auto* scriptedActivityComponent = self->GetComponent<ScriptedActivityComponent>();

	if (scriptedActivityComponent == nullptr) {
		return;
	}

	if (scriptedActivityComponent->GetActivityPlayerData(user->GetObjectID()) != nullptr) {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"exit", 0, 0, LWOOBJID_EMPTY, "", user->GetSystemAddress());
	} else {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"start", 0, 0, LWOOBJID_EMPTY, "", user->GetSystemAddress());
	}
}

void NpcAgCourseStarter::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	auto* scriptedActivityComponent = self->GetComponent<ScriptedActivityComponent>();

	if (scriptedActivityComponent == nullptr) {
		return;
	}

	if (identifier == u"player_dialog_cancel_course" && button == 1) {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"stop_timer", 0, 0, LWOOBJID_EMPTY, "", sender->GetSystemAddress());

		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"cancel_timer", 0, 0, LWOOBJID_EMPTY, "", sender->GetSystemAddress());

		scriptedActivityComponent->RemoveActivityPlayerData(sender->GetObjectID());

		Game::entityManager->SerializeEntity(self);
	} else if (identifier == u"player_dialog_start_course" && button == 1) {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"start_timer", 0, 0, LWOOBJID_EMPTY, "", sender->GetSystemAddress());

		GameMessages::SendActivityStart(self->GetObjectID(), sender->GetSystemAddress());

		auto* data = scriptedActivityComponent->AddActivityPlayerData(sender->GetObjectID());
		if (data->values[1] != 0) return;

		const auto raceStartTime = std::chrono::steady_clock::now() - Game::server->GetStartTime()
			+ std::chrono::seconds(4);  // Offset for starting timer
		const auto fRaceStartTime = std::chrono::duration<float, std::ratio<1>>(raceStartTime).count();
		data->values[1] = fRaceStartTime;
		LOG_DEBUG("Race started at: %0.f s", fRaceStartTime);

		Game::entityManager->SerializeEntity(self);
	} else if (identifier == u"FootRaceCancel") {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"stop_timer", 0, 0, LWOOBJID_EMPTY, "", sender->GetSystemAddress());

		if (scriptedActivityComponent->GetActivityPlayerData(sender->GetObjectID()) != nullptr) {
			GameMessages::SendNotifyClientObject(self->GetObjectID(), u"exit", 0, 0, LWOOBJID_EMPTY, "", sender->GetSystemAddress());
		} else {
			GameMessages::SendNotifyClientObject(self->GetObjectID(), u"start", 0, 0, LWOOBJID_EMPTY, "", sender->GetSystemAddress());
		}

		scriptedActivityComponent->RemoveActivityPlayerData(sender->GetObjectID());
	}
}

void NpcAgCourseStarter::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	auto* scriptedActivityComponent = self->GetComponent<ScriptedActivityComponent>();
	if (scriptedActivityComponent == nullptr) return;

	auto* data = scriptedActivityComponent->GetActivityPlayerData(sender->GetObjectID());
	if (data == nullptr) return;

	if (args == "course_cancel") {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"cancel_timer", 0, 0,
			LWOOBJID_EMPTY, "", sender->GetSystemAddress());
		scriptedActivityComponent->RemoveActivityPlayerData(sender->GetObjectID());
	} else if (args == "course_finish") {
		const auto raceEndTime = std::chrono::steady_clock::now() - Game::server->GetStartTime();
		const auto fRaceEndTime = std::chrono::duration<float, std::ratio<1>>(raceEndTime).count();
		const auto raceTimeElapsed = fRaceEndTime - data->values[1];

		data->values[2] = raceTimeElapsed;
		LOG_DEBUG("Race time elapsed: %0.f s", raceTimeElapsed);

		auto* missionComponent = sender->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->ForceProgressTaskType(1884, 1, 1, false);
			missionComponent->Progress(eMissionTaskType::PERFORM_ACTIVITY, -raceTimeElapsed, self->GetObjectID(),
				"performact_time");
		}

		Game::entityManager->SerializeEntity(self);
		LeaderboardManager::SaveScore(sender->GetObjectID(), scriptedActivityComponent->GetActivityID(), raceTimeElapsed);

		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"ToggleLeaderBoard",
			scriptedActivityComponent->GetActivityID(), 0, sender->GetObjectID(),
			"", sender->GetSystemAddress());
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"stop_timer", 1, raceTimeElapsed, LWOOBJID_EMPTY, "",
			sender->GetSystemAddress());

		scriptedActivityComponent->RemoveActivityPlayerData(sender->GetObjectID());
	}
}
