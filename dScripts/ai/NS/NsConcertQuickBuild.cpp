#include "NsConcertQuickBuild.h"
#include "EntityManager.h"
#include "NsConcertChoiceBuildManager.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "MissionComponent.h"

const float NsConcertQuickBuild::resetTime = 40.0f;
const float NsConcertQuickBuild::resetBlinkTime = 6.0f;
const float NsConcertQuickBuild::resetStageTime = 66.5f;
const float NsConcertQuickBuild::resetActivatorTime = 30.0f;
const std::map<LOT, QuickBuildSet> NsConcertQuickBuild::quickBuildSets{
	{5846, QuickBuildSet {"laser", {"discoball", "discofloor", "stagelights", "spotlight"}}},
	{5847, QuickBuildSet {"spotlight", {"spotlight", "stagelights"}}},
	{5848, QuickBuildSet {"rocket", {"flamethrower"}}},
	{5845, QuickBuildSet {"speaker", {"speaker", "speakerHill", "stagelights", "spotlight"}}}
};

const std::map<std::string, std::string> NsConcertQuickBuild::quickBuildFX{
		{"discoball", "effectsDiscoball"},
		{"speaker", "effectsShell"},
		{"speakerHill", "effectsHill"},
		{"spotlight", "effectsHill"},
		{"discofloor", "effectsShell"},
		{"flamethrower", "effectsShell"},
		{"stagelights", "effectsShell"}
};

std::vector<LWOOBJID> NsConcertQuickBuild::finishedQuickBuilds = {};

void NsConcertQuickBuild::OnStartup(Entity* self) {
	const auto groups = self->GetGroups();
	if (groups.empty())
		return;

	// Groups are of the form Concert_Laser_QB_1, Concert_Laser_QB_2, etc.
	auto group = groups.at(0);
	const auto splitGroup = GeneralUtils::SplitString(group, '_');
	if (splitGroup.size() < 4)
		return;

	// Get the manager of the crate of this quick build
	const auto groupNumber = std::stoi(splitGroup.at(3));
	const auto managerObjects = EntityManager::Instance()->GetEntitiesInGroup("CB_" + std::to_string(groupNumber));
	if (managerObjects.empty())
		return;

	auto* managerObject = managerObjects.at(0);
	self->SetVar<LWOOBJID>(u"managerObject", managerObject->GetObjectID());
	self->SetVar<int32_t>(u"groupNumber", groupNumber);

	// Makes the quick build blink after a certain amount of time
	self->AddCallbackTimer(GetBlinkTime(resetActivatorTime), [self]() {
		self->SetNetworkVar<float>(u"startEffect", NsConcertQuickBuild::GetBlinkTime(resetActivatorTime));
		});

	// Destroys the quick build after a while if it wasn't built
	self->AddCallbackTimer(resetActivatorTime, [self]() {
		self->SetNetworkVar<float>(u"startEffect", -1.0f);
		self->Smash(self->GetObjectID(), SILENT);
		});
}

float NsConcertQuickBuild::GetBlinkTime(float time) {
	return time <= NsConcertQuickBuild::resetBlinkTime ? 1.0f : time - NsConcertQuickBuild::resetBlinkTime;
}

void NsConcertQuickBuild::OnDie(Entity* self, Entity* killer) {
	auto* managerObject = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"managerObject"));
	if (managerObject) {
		managerObject->CancelAllTimers();
		managerObject->AddCallbackTimer(1.0f, [managerObject]() {
			NsConcertChoiceBuildManager::SpawnCrate(managerObject);
			});
	}

	auto position = std::find(finishedQuickBuilds.begin(), finishedQuickBuilds.end(), self->GetObjectID());
	if (position != finishedQuickBuilds.end())
		finishedQuickBuilds.erase(position);
}

void NsConcertQuickBuild::OnRebuildComplete(Entity* self, Entity* target) {
	const auto groupNumber = self->GetVar<int32_t>(u"groupNumber");
	finishedQuickBuilds.push_back(self->GetObjectID());
	self->SetNetworkVar<float>(u"startEffect", -1.0f);

	ProgressStageCraft(self, target);

	// Find all the quick build objects of the same lot
	auto finishedQuickBuildObjects = std::vector<Entity*>();
	for (auto quickBuildID : finishedQuickBuilds) {
		const auto quickBuildObject = EntityManager::Instance()->GetEntity(quickBuildID);
		if (quickBuildObject && quickBuildObject->GetLOT() == self->GetLOT()) {
			quickBuildObject->SetVar<LWOOBJID>(u"Player_" + (GeneralUtils::to_u16string(groupNumber)), target->GetObjectID());
			finishedQuickBuildObjects.push_back(quickBuildObject);
		}
	}

	// If all 4 sets were built, do cool stuff
	if (finishedQuickBuildObjects.size() >= 4) {

		// Move all the platforms so the user can collect the imagination brick
		const auto movingPlatforms = EntityManager::Instance()->GetEntitiesInGroup("ConcertPlatforms");
		for (auto* movingPlatform : movingPlatforms) {
			auto* component = movingPlatform->GetComponent<MovingPlatformComponent>();
			if (component) {
				component->WarpToWaypoint(component->GetLastWaypointIndex());

				movingPlatform->AddCallbackTimer(resetStageTime, [movingPlatform, component]() {
					component->WarpToWaypoint(0);
					});
			}
		}

		ProgressLicensedTechnician(self);

		// Reset all timers for the quickbuilds and make them indestructible
		for (auto quickBuild : finishedQuickBuildObjects) {
			quickBuild->SetNetworkVar<float>(u"startEffect", -1.0f);
			quickBuild->CancelAllTimers();

			// Indicate that the stage will reset
			quickBuild->AddCallbackTimer(GetBlinkTime(resetStageTime), [quickBuild]() {
				quickBuild->SetNetworkVar<float>(u"startEffect", GetBlinkTime(resetTime));
				});

			// Reset the stage
			quickBuild->AddCallbackTimer(resetStageTime, [quickBuild]() {
				CancelEffects(quickBuild);
				quickBuild->SetNetworkVar<float>(u"startEffect", -1);
				quickBuild->Smash();
				});

			auto* destroyableComponent = quickBuild->GetComponent<DestroyableComponent>();
			if (destroyableComponent)
				destroyableComponent->SetFaction(-1);
		}

		UpdateEffects(self);
		return;
	}

	// If not all 4 sets were built, reset the timers that were set on spawn
	self->CancelAllTimers();

	// Makes the quick build blink after a certain amount of time
	self->AddCallbackTimer(GetBlinkTime(resetTime), [self]() {
		self->SetNetworkVar<float>(u"startEffect", NsConcertQuickBuild::GetBlinkTime(resetActivatorTime));
		});

	// Destroys the quick build after a while if it wasn't built
	self->AddCallbackTimer(resetTime, [self]() {
		self->SetNetworkVar<float>(u"startEffect", -1.0f);
		self->Smash(self->GetObjectID());
		});
}

void NsConcertQuickBuild::ProgressStageCraft(Entity* self, Entity* player) {
	auto* missionComponent = player->GetComponent<MissionComponent>();
	if (missionComponent) {

		// Has to be forced as to not accidentally trigger the licensed technician achievement
		switch (self->GetLOT()) {
		case 5845:
			missionComponent->ForceProgress(283, 432, 5845);
			break;
		case 5846:
			missionComponent->ForceProgress(283, 433, 5846);
			break;
		case 5847:
			missionComponent->ForceProgress(283, 434, 5847);
			break;
		case 5848:
			missionComponent->ForceProgress(283, 435, 5848);
			break;
		default:
			break;
		}
	}
}

void NsConcertQuickBuild::ProgressLicensedTechnician(Entity* self) {
	for (auto i = 1; i < 5; i++) {
		const auto playerID = self->GetVar<LWOOBJID>(u"Player_" + (GeneralUtils::to_u16string(i)));
		if (playerID != LWOOBJID_EMPTY) {
			const auto player = EntityManager::Instance()->GetEntity(playerID);
			if (player) {
				auto playerMissionComponent = player->GetComponent<MissionComponent>();
				if (playerMissionComponent)
					playerMissionComponent->ForceProgress(598, 903, self->GetLOT());
			}
		}
	}
}

void NsConcertQuickBuild::UpdateEffects(Entity* self) {
	CancelEffects(self);

	auto setIterator = quickBuildSets.find(self->GetLOT());
	if (setIterator == quickBuildSets.end())
		return;

	for (const auto& effectName : setIterator->second.effects) {
		const auto effectObjects = EntityManager::Instance()->GetEntitiesInGroup(quickBuildFX.at(effectName));
		for (auto* effectObject : effectObjects) {
			GameMessages::SendPlayFXEffect(effectObject, 0, GeneralUtils::ASCIIToUTF16(effectName),
				effectName + "Effect", LWOOBJID_EMPTY, 1, 1, true);
		}
	}
}

void NsConcertQuickBuild::CancelEffects(Entity* self) {
	auto setIterator = quickBuildSets.find(self->GetLOT());
	if (setIterator == quickBuildSets.end())
		return;

	for (const auto& effectName : setIterator->second.effects) {
		const auto effectObjects = EntityManager::Instance()->GetEntitiesInGroup(quickBuildFX.at(effectName));
		for (auto* effectObject : effectObjects) {
			GameMessages::SendStopFXEffect(effectObject, true, effectName + "Effect");
		}
	}
}
