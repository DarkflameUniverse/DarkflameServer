#include "FvFlyingCreviceDragon.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "GeneralUtils.h"

void FvFlyingCreviceDragon::OnStartup(Entity* self) {
	self->AddTimer("waypoint", 5);
}

void FvFlyingCreviceDragon::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "waypoint") {
		auto point = self->GetVar<int32_t>(u"waypoint");

		if (point >= 20) {
			point = 0;
		}

		self->SetVar<int32_t>(u"waypoint", point + 1);

		self->AddTimer("waypoint", 5);

		OnArrived(self);

		return;
	}

	std::string groupName = "";

	if (timerName == "platform1attack") {
		groupName = "dragonFireballs1";
	} else if (timerName == "platform3attack") {
		groupName = "dragonFireballs3";
	}

	const auto& group = EntityManager::Instance()->GetEntitiesInGroup(groupName);

	if (group.empty()) {
		return;
	}

	auto* skillComponent = group[0]->GetComponent<SkillComponent>();

	if (skillComponent != nullptr) {
		skillComponent->CalculateBehavior(762, 12506, LWOOBJID_EMPTY, true);
	}

	auto minionCount = 1;
	for (size_t i = 1; i < group.size(); i++) {
		if (minionCount == 4) {
			return;
		}

		if (/*GeneralUtils::GenerateRandomNumber<int32_t>(1, 5) > 3*/ true) {
			skillComponent = group[i]->GetComponent<SkillComponent>();

			if (skillComponent != nullptr) {
				skillComponent->CalculateBehavior(762, 12506, LWOOBJID_EMPTY);

				++minionCount;
			}
		}
	}
}

void FvFlyingCreviceDragon::OnArrived(Entity* self) {
	auto point = self->GetVar<int32_t>(u"waypoint");

	if (point == 4) {
		GameMessages::SendPlayAnimation(self, u"attack1", 2);
		self->AddTimer("platform1attack", 1.75f);
	} else if (point == 12) {
		GameMessages::SendPlayAnimation(self, u"attack2", 2);

		const auto& group2 = EntityManager::Instance()->GetEntitiesInGroup("dragonFireballs2");

		if (group2.empty()) {
			return;
		}

		auto* skillComponent = group2[0]->GetComponent<SkillComponent>();

		if (skillComponent != nullptr) {
			skillComponent->CalculateBehavior(762, 12506, LWOOBJID_EMPTY);
		}

		auto minionCount = 1;
		for (size_t i = 1; i < group2.size(); i++) {
			if (minionCount == 4) {
				return;
			}

			if (GeneralUtils::GenerateRandomNumber<int32_t>(1, 5) > 3) {
				skillComponent = group2[i]->GetComponent<SkillComponent>();

				if (skillComponent != nullptr) {
					skillComponent->CalculateBehavior(762, 12506, LWOOBJID_EMPTY, true);

					++minionCount;
				}
			}
		}
	} else if (point == 16) {
		GameMessages::SendPlayAnimation(self, u"attack3", 2);
		self->AddTimer("platform3attack", 0.5f);
	}
}
