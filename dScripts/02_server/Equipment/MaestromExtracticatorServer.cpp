#include "MaestromExtracticatorServer.h"
#include "GameMessages.h"
#include "GeneralUtils.h"
#include "EntityManager.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"
#include "RenderComponent.h"

void MaestromExtracticatorServer::OnStartup(Entity* self) {
	self->AddTimer("PlayFail", RenderComponent::PlayAnimation(self, failAnim));
	self->AddTimer("RemoveSample", destroyAfterNoSampleTime);
}

void MaestromExtracticatorServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (sender == nullptr) return;

	if (args == "attemptCollection") {
		Entity* player = EntityManager::Instance()->GetEntity(self->GetSpawnerID());
		if (!player) return;

		auto missionComponent = player->GetComponent<MissionComponent>();
		if (missionComponent == nullptr) return;

		missionComponent->Progress(eMissionTaskType::SMASH, 14718);
		CollectSample(self, sender->GetObjectID());
		sender->ScheduleKillAfterUpdate();
	}
}

void MaestromExtracticatorServer::CollectSample(Entity* self, LWOOBJID sampleObj) {
	self->AddTimer("RemoveSample", PlayAnimAndReturnTime(self, collectAnim));
}

float MaestromExtracticatorServer::PlayAnimAndReturnTime(Entity* self, std::string animID) {
	return RenderComponent::PlayAnimation(self, animID);
}

void MaestromExtracticatorServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "RemoveSample") {
		self->ScheduleKillAfterUpdate();
	} else if (timerName == "PlayFail") {
		RenderComponent::PlayAnimation(self, failAnim);
	}
}
