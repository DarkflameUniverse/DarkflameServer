#include "LegoDieRoll.h"
#include "Entity.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eMissionState.h"

void LegoDieRoll::OnStartup(Entity* self) {
	self->AddTimer("DoneRolling", 10.0f);
	self->AddTimer("ThrowDice", LegoDieRoll::animTime);
}

void LegoDieRoll::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "DoneRolling") {
		self->Smash(self->GetObjectID(), eKillType::SILENT);
	} else if (timerName == "ThrowDice") {
		int dieRoll = GeneralUtils::GenerateRandomNumber<int>(1, 6);

		switch (dieRoll) {
		case 1:
			RenderComponent::PlayAnimation(self, u"roll-die-1");
			break;
		case 2:
			RenderComponent::PlayAnimation(self, u"roll-die-2");
			break;
		case 3:
			RenderComponent::PlayAnimation(self, u"roll-die-3");
			break;
		case 4:
			RenderComponent::PlayAnimation(self, u"roll-die-4");
			break;
		case 5:
			RenderComponent::PlayAnimation(self, u"roll-die-5");
			break;
		case 6:
		{
			RenderComponent::PlayAnimation(self, u"roll-die-6");
			// tracking the It's Truly Random Achievement
			auto* owner = self->GetOwner();
			auto* missionComponent = owner->GetComponent<MissionComponent>();

			if (missionComponent != nullptr) {
				const auto rollMissionState = missionComponent->GetMissionState(756);
				if (rollMissionState == eMissionState::ACTIVE) {
					missionComponent->ForceProgress(756, 1103, 1);
				}
			}
			break;
		}
		default:
			break;
		}
	}
}
