#include "MaestromExtracticatorServer.h"
#include "GameMessages.h"
#include "GeneralUtils.h"
#include "EntityManager.h"
#include "MissionComponent.h"

void MaestromExtracticatorServer::OnStartup(Entity* self) {
	//self:SetNetworkVar("current_anim", failAnim)
	GameMessages::SendPlayAnimation(self, GeneralUtils::ASCIIToUTF16(failAnim));

	self->AddTimer("PlayFail", defaultTime);
	self->AddTimer("RemoveSample", destroyAfterNoSampleTime);
}

void MaestromExtracticatorServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (sender == nullptr)
		return;

	if (args == "attemptCollection") {
		Entity* player = EntityManager::Instance()->GetEntity(self->GetSpawnerID());
		if (!player) return;

		auto missionComponent = player->GetComponent<MissionComponent>();
		if (missionComponent == nullptr) return;

		missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_SMASH, 14718);
		CollectSample(self, sender->GetObjectID());
		sender->ScheduleKillAfterUpdate();
	}
}

void MaestromExtracticatorServer::CollectSample(Entity* self, LWOOBJID sampleObj) {
	PlayAnimAndReturnTime(self, collectAnim);
	self->AddTimer("RemoveSample", defaultTime);
}

void MaestromExtracticatorServer::PlayAnimAndReturnTime(Entity* self, std::string animID) {
	GameMessages::SendPlayAnimation(self, GeneralUtils::ASCIIToUTF16(animID));
}

void MaestromExtracticatorServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "RemoveSample") {
		self->ScheduleKillAfterUpdate();
	}

	if (timerName == "PlayFail") {
		GameMessages::SendPlayAnimation(self, GeneralUtils::ASCIIToUTF16(failAnim));
	}
}
