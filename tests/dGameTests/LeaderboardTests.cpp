#include "LeaderboardManager.h"

#include "BitStream.h"
#include "GameDependencies.h"
#include "Metrics.hpp"
#include <gtest/gtest.h>

class LeaderboardTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}
	void TearDown() override {
		TearDownDependencies();
	}

	void TestLeaderboard(Leaderboard& leaderboard, int32_t entries) {
		bitStream.Reset();
		Metrics::StartMeasurement(MetricVariable::Leaderboard);
		for (int32_t i = 0; i < MAX_MEASURMENT_POINTS; i++) leaderboard.Serialize(&bitStream);
		Metrics::EndMeasurement(MetricVariable::Leaderboard);

		auto timePassed = Metrics::GetMetric(MetricVariable::Leaderboard)->average;
		Game::logger->Log("LeaderboardManager", "average time passed for %i leaderboard entries is %lluns", entries, timePassed);
		bitStream.Reset();
	}

	void RunTests(uint32_t gameID, Leaderboard::Type type, Leaderboard::InfoType infoType) {
		Game::logger->Log("LeaderboardTests", "Testing leaderboard %i for Serialize speed", infoType);
		Leaderboard leaderboard(gameID, infoType, false, 14231, type);
		leaderboard.SetupLeaderboard();
		leaderboard.Serialize(&bitStream);
		// TestLeaderboard(leaderboard, 1);
		// TestLeaderboard(leaderboard, 10);
		// TestLeaderboard(leaderboard, 100);
		// TestLeaderboard(leaderboard, 1000);
	}

	CBITSTREAM;
};

/**
 * Initial metrics
 * 19: [12-04-23 23:56:31] [LeaderboardManager] average time passed for 1 leaderboard entries is 1671700ns
 * 19: [12-04-23 23:56:31] [LeaderboardManager] average time passed for 10 leaderboard entries is 8388900ns
 * 19: [12-04-23 23:56:31] [LeaderboardManager] average time passed for 100 leaderboard entries is 54680133ns
 * 19: [12-04-23 23:56:33] [LeaderboardManager] average time passed for 1000 leaderboard entries is 506289325ns

 * Only do each std::to_string once
 * 19: [12-04-23 23:57:31] [LeaderboardManager] average time passed for 1 leaderboard entries is 1472700ns
 * 19: [12-04-23 23:57:31] [LeaderboardManager] average time passed for 10 leaderboard entries is 7035650ns
 * 19: [12-04-23 23:57:31] [LeaderboardManager] average time passed for 100 leaderboard entries is 45147466ns
 * 19: [12-04-23 23:57:33] [LeaderboardManager] average time passed for 1000 leaderboard entries is 435724550ns
 * 
 * Only do Result[0].Row[index] once
 * 19: [12-04-23 23:59:43] [LeaderboardManager] average time passed for 1 leaderboard entries is 1357700ns
 * 19: [12-04-23 23:59:43] [LeaderboardManager] average time passed for 10 leaderboard entries is 6635350ns
 * 19: [12-04-23 23:59:43] [LeaderboardManager] average time passed for 100 leaderboard entries is 40247800ns
 * 19: [12-04-23 23:59:45] [LeaderboardManager] average time passed for 1000 leaderboard entries is 400965900ns
 * 
 * Switch to ostringstream
 * 19: [13-04-23 00:24:44] [LeaderboardManager] average time passed for 1 leaderboard entries is 1334300ns
 * 19: [13-04-23 00:24:44] [LeaderboardManager] average time passed for 10 leaderboard entries is 5566250ns
 * 19: [13-04-23 00:24:44] [LeaderboardManager] average time passed for 100 leaderboard entries is 34640066ns
 * 19: [13-04-23 00:24:46] [LeaderboardManager] average time passed for 1000 leaderboard entries is 357226950ns
 * 
 * No more std::to_string and revert "Only do Result[0].Row[index] once"
 * 19: [13-04-23 00:39:18] [LeaderboardManager] average time passed for 1 leaderboard entries is 979200ns
 * 19: [13-04-23 00:39:18] [LeaderboardManager] average time passed for 10 leaderboard entries is 4053350ns
 * 19: [13-04-23 00:39:18] [LeaderboardManager] average time passed for 100 leaderboard entries is 24785233ns
 * 19: [13-04-23 00:39:19] [LeaderboardManager] average time passed for 1000 leaderboard entries is 279457375ns
 */

TEST_F(LeaderboardTests, LeaderboardSpeedTest) {
	RunTests(1864, Leaderboard::Type::ShootingGallery , Leaderboard::InfoType::Top);
	// RunTests(1864, Leaderboard::Type::ShootingGallery, Leaderboard::InfoType::MyStanding);
	// RunTests(1864, Leaderboard::Type::ShootingGallery, Leaderboard::InfoType::Friends);
	LeaderboardManager::Instance().SaveScore(14231, 1864, Leaderboard::Type::ShootingGallery, 3, 53002, 15.0f, 100);
	// RunTests(0, Leaderboard::Type::Racing);
	LeaderboardManager::Instance().SaveScore(14231, 0, Leaderboard::Type::Racing, 3, 259.0f, 250.0f, true);
	// RunTests(0, Leaderboard::Type::MonumentRace);
	LeaderboardManager::Instance().SaveScore(14231, 0, Leaderboard::Type::MonumentRace, 1, 149);
	// RunTests(0, Leaderboard::Type::FootRace);
	LeaderboardManager::Instance().SaveScore(14231, 0, Leaderboard::Type::FootRace, 1, 151);
	// RunTests(0, Leaderboard::Type::UnusedLeaderboard4);
	LeaderboardManager::Instance().SaveScore(14231, 0, Leaderboard::Type::UnusedLeaderboard4, 1, 101);
	// RunTests(0, Leaderboard::Type::Survival);
	LeaderboardManager::Instance().SaveScore(14231, 0, Leaderboard::Type::Survival, 2, 3001, 15);
	// RunTests(0, Leaderboard::Type::SurvivalNS);
	LeaderboardManager::Instance().SaveScore(14231, 0, Leaderboard::Type::SurvivalNS, 2, 301, 15);
	// RunTests(0, Leaderboard::Type::Donations);
	LeaderboardManager::Instance().SaveScore(14231, 0, Leaderboard::Type::Donations, 1, 300001);
}
