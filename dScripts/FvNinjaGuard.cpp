#include "FvNinjaGuard.h"
#include "GameMessages.h"
#include "MissionComponent.h"

void FvNinjaGuard::OnStartup(Entity* self) {
	if (self->GetLOT() == 7412) {
		m_LeftGuard = self->GetObjectID();
	} else if (self->GetLOT() == 11128) {
		m_RightGuard = self->GetObjectID();
	}
}

void FvNinjaGuard::OnEmoteReceived(Entity* self, const int32_t emote, Entity* target) {
	if (emote != 392) {
		GameMessages::SendPlayAnimation(self, u"no");

		return;
	}

	GameMessages::SendPlayAnimation(self, u"scared");

	auto* missionComponent = target->GetComponent<MissionComponent>();

	if (missionComponent != nullptr && missionComponent->HasMission(737)) {
		missionComponent->ForceProgressTaskType(737, 5, 1, false);
	}

	if (self->GetLOT() == 7412) {
		auto* rightGuard = EntityManager::Instance()->GetEntity(m_RightGuard);

		if (rightGuard != nullptr) {
			GameMessages::SendPlayAnimation(rightGuard, u"laugh_rt");
		}
	} else if (self->GetLOT() == 11128) {
		auto* leftGuard = EntityManager::Instance()->GetEntity(m_LeftGuard);

		if (leftGuard != nullptr) {
			GameMessages::SendPlayAnimation(leftGuard, u"laugh_lt");
		}
	}
}

