#include "ActivityManager.h"
#include "EntityManager.h"
#include "ScriptedActivityComponent.h"
#include "LeaderboardManager.h"
#include "GameMessages.h"
#include <algorithm>
#include "dLogger.h"

bool ActivityManager::IsPlayerInActivity(Entity* self, LWOOBJID playerID) {
	const auto* sac = self->GetComponent<ScriptedActivityComponent>();
	return sac != nullptr && sac->IsPlayedBy(playerID);
}

void ActivityManager::UpdatePlayer(Entity* self, LWOOBJID playerID, const bool remove) {
	auto* sac = self->GetComponent<ScriptedActivityComponent>();
	if (sac == nullptr)
		return;

	if (remove) {
		sac->PlayerRemove(playerID);
	} else {
		auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player != nullptr) {
			sac->PlayerJoin(player);
			SetActivityScore(self, playerID, 0);
		}
	}
}

void ActivityManager::SetActivityScore(Entity* self, LWOOBJID playerID, uint32_t score) {
	SetActivityValue(self, playerID, 0, score);
}

void ActivityManager::SetActivityValue(Entity* self, const LWOOBJID playerID, const uint32_t valueIndex,
	const float_t value) {
	auto* sac = self->GetComponent<ScriptedActivityComponent>();
	if (sac == nullptr)
		return;

	sac->SetActivityValue(playerID, valueIndex, value);
}

float_t ActivityManager::GetActivityValue(Entity* self, const LWOOBJID playerID, const uint32_t valueIndex) {
	auto* sac = self->GetComponent<ScriptedActivityComponent>();
	if (sac == nullptr)
		return -1.0f;

	return sac->GetActivityValue(playerID, valueIndex);
}

void ActivityManager::StopActivity(Entity* self, const LWOOBJID playerID, const uint32_t score,
	const uint32_t value1, const uint32_t value2, bool quit) {
	int32_t gameID = 0;

	auto* sac = self->GetComponent<ScriptedActivityComponent>();
	if (sac == nullptr) {
		gameID = self->GetLOT();
	} else {
		gameID = sac->GetActivityID();
	}

	if (quit) {
		UpdatePlayer(self, playerID, true);
	} else {
		auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player == nullptr)
			return;

		SetActivityScore(self, playerID, score);
		SetActivityValue(self, playerID, 1, value1);
		SetActivityValue(self, playerID, 2, value2);

		LootGenerator::Instance().GiveActivityLoot(player, self, gameID, CalculateActivityRating(self, playerID));

		// Save the new score to the leaderboard and show the leaderboard to the player
		LeaderboardManager::SaveScore(playerID, gameID, score, value1);
		const auto* leaderboard = LeaderboardManager::GetLeaderboard(gameID, InfoType::Standings,
			false, player->GetObjectID());
		GameMessages::SendActivitySummaryLeaderboardData(self->GetObjectID(), leaderboard, player->GetSystemAddress());
		delete leaderboard;

		// Makes the leaderboard show up for the player
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"ToggleLeaderBoard",
			gameID, 0, playerID, "",
			player->GetSystemAddress());

		if (sac != nullptr) {
			sac->PlayerRemove(player->GetObjectID());
		}
	}
}

bool ActivityManager::TakeActivityCost(const Entity* self, const LWOOBJID playerID) {
	auto* sac = self->GetComponent<ScriptedActivityComponent>();
	if (sac == nullptr)
		return false;

	auto* player = EntityManager::Instance()->GetEntity(playerID);
	if (player == nullptr)
		return false;

	return sac->TakeCost(player);
}

uint32_t ActivityManager::CalculateActivityRating(Entity* self, const LWOOBJID playerID) {
	auto* sac = self->GetComponent<ScriptedActivityComponent>();
	if (sac == nullptr)
		return 0;

	return sac->GetInstance(playerID)->GetParticipants().size();
}

uint32_t ActivityManager::GetActivityID(const Entity* self) {
	auto* sac = self->GetComponent<ScriptedActivityComponent>();
	return sac != nullptr ? sac->GetActivityID() : 0;
}

void ActivityManager::GetLeaderboardData(Entity* self, const LWOOBJID playerID, const uint32_t activityID, uint32_t numResults) {
	LeaderboardManager::SendLeaderboard(activityID, Standings, false, self->GetObjectID(), playerID);
}

void ActivityManager::ActivityTimerStart(Entity* self, const std::string& timerName, const float_t updateInterval,
	const float_t stopTime) {
	auto* timer = new ActivityTimer{ timerName, updateInterval, stopTime };
	activeTimers.push_back(timer);

	Game::logger->Log("ActivityManager", "Starting timer '%s', %f, %f", timerName.c_str(), updateInterval, stopTime);

	self->AddTimer(GetPrefixedName(timer->name), timer->updateInterval);
}

void ActivityManager::ActivityTimerStopAllTimers(Entity* self) {
	for (auto* timer : activeTimers) {
		self->CancelTimer(GetPrefixedName(timer->name));
		delete timer;
	}

	activeTimers.clear();
}

float_t ActivityManager::ActivityTimerGetCurrentTime(Entity* self, const std::string& timerName) const {
	auto* timer = GetTimer(timerName);
	return timer != nullptr ? timer->runTime : 0.0f;
}

int32_t ActivityManager::GetGameID(Entity* self) const {
	int32_t gameID = 0;

	auto* sac = self->GetComponent<ScriptedActivityComponent>();
	if (sac == nullptr) {
		gameID = self->GetLOT();
	} else {
		gameID = sac->GetActivityID();
	}

	return gameID;
}

float_t ActivityManager::ActivityTimerGetRemainingTime(Entity* self, const std::string& timerName) const {
	auto* timer = GetTimer(timerName);
	return timer != nullptr ? std::min(timer->stopTime - timer->runTime, 0.0f) : 0.0f;
}

void ActivityManager::ActivityTimerReset(Entity* self, const std::string& timerName) {
	auto* timer = GetTimer(timerName);
	if (timer != nullptr) {
		timer->runTime = 0.0f;
	}
}

ActivityTimer* ActivityManager::GetTimer(const std::string& name) const {
	for (auto* timer : activeTimers) {
		if (timer->name == name)
			return timer;
	}

	return nullptr;
}

void ActivityManager::ActivityTimerStop(Entity* self, const std::string& timerName) {
	auto* timer = GetTimer(timerName);
	if (timer != nullptr) {
		self->CancelTimer(GetPrefixedName(timer->name));

		activeTimers.erase(std::remove(activeTimers.begin(), activeTimers.end(), timer),
			activeTimers.end());
		delete timer;
	}
}

std::string ActivityManager::GetPrefixedName(const std::string& name) const {
	return TimerPrefix + "_" + name;
}

void ActivityManager::OnTimerDone(Entity* self, std::string timerName) {
	auto nameSplit = GeneralUtils::SplitString(timerName, '_');
	if (nameSplit.size() > 1 && nameSplit.at(0) == TimerPrefix) {
		const auto& activityTimerName = nameSplit.at(1);
		auto* timer = GetTimer(activityTimerName);

		if (timer != nullptr) {
			timer->runTime += timer->updateInterval;

			if (timer->stopTime != -1.0f && timer->runTime >= timer->stopTime) {
				activeTimers.erase(std::remove(activeTimers.begin(), activeTimers.end(), timer),
					activeTimers.end());
				delete timer;
				Game::logger->Log("ActivityManager", "Executing timer '%s'", activityTimerName.c_str());
				OnActivityTimerDone(self, activityTimerName);
			} else {
				Game::logger->Log("ActivityManager", "Updating timer '%s'", activityTimerName.c_str());
				OnActivityTimerUpdate(self, timer->name, timer->stopTime - timer->runTime, timer->runTime);
				self->AddTimer(GetPrefixedName(timer->name), timer->updateInterval);
			}
		}
	}
}
