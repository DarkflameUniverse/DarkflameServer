#include "NpcAgCourseStarter.h"
#include "EntityManager.h"
#include "ScriptedActivityComponent.h"
#include "GameMessages.h"
#include "LeaderboardManager.h"
#include "MissionComponent.h"
#include <ctime>

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

		EntityManager::Instance()->SerializeEntity(self);
	} else if (identifier == u"player_dialog_start_course" && button == 1) {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"start_timer", 0, 0, LWOOBJID_EMPTY, "", sender->GetSystemAddress());

		GameMessages::SendActivityStart(self->GetObjectID(), sender->GetSystemAddress());

		auto* data = scriptedActivityComponent->AddActivityPlayerData(sender->GetObjectID());

		if (data->values[1] != 0) return;

		time_t startTime = std::time(0) + 4; // Offset for starting timer

		data->values[1] = *(float*)&startTime;

		EntityManager::Instance()->SerializeEntity(self);
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

void NpcAgCourseStarter::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	auto* scriptedActivityComponent = self->GetComponent<ScriptedActivityComponent>();
	if (scriptedActivityComponent == nullptr)
		return;

	auto* data = scriptedActivityComponent->GetActivityPlayerData(sender->GetObjectID());
	if (data == nullptr)
		return;

	if (args == "course_cancel") {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"cancel_timer", 0, 0,
			LWOOBJID_EMPTY, "", sender->GetSystemAddress());
		scriptedActivityComponent->RemoveActivityPlayerData(sender->GetObjectID());
	} else if (args == "course_finish") {
		time_t endTime = std::time(0);
		time_t finish = (endTime - *(time_t*)&data->values[1]);

		data->values[2] = *(float*)&finish;

		auto* missionComponent = sender->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->ForceProgressTaskType(1884, 1, 1, false);
			missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_MINIGAME, -finish, self->GetObjectID(),
				"performact_time");
		}

		EntityManager::Instance()->SerializeEntity(self);
		LeaderboardManager::SaveScore(sender->GetObjectID(), scriptedActivityComponent->GetActivityID(),
			0, (uint32_t)finish);

		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"ToggleLeaderBoard",
			scriptedActivityComponent->GetActivityID(), 0, sender->GetObjectID(),
			"", sender->GetSystemAddress());
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"stop_timer", 1, finish, LWOOBJID_EMPTY, "",
			sender->GetSystemAddress());

		scriptedActivityComponent->RemoveActivityPlayerData(sender->GetObjectID());
	}
}
