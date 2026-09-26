#include <gtest/gtest.h>

#include "PlayerReports.h"
#include "DashboardNotify.h"

TEST(PlayerReportsLimitTests, FewPerMinuteAndPerDay) {
	PlayerReports::RateLimit limit;
	for (size_t i = 0; i < PlayerReports::RateLimit::PER_MINUTE; i++) EXPECT_TRUE(limit.Allow(1, 1000));
	EXPECT_FALSE(limit.Allow(1, 1010));  // same minute
	EXPECT_TRUE(limit.Allow(2, 1010));   // another account is separate
	EXPECT_TRUE(limit.Allow(1, 1061));   // a minute later

	PlayerReports::RateLimit daily;
	int64_t time = 0;
	for (size_t i = 0; i < PlayerReports::RateLimit::PER_DAY; i++, time += 61) EXPECT_TRUE(daily.Allow(5, time));
	EXPECT_FALSE(daily.Allow(5, time));
	EXPECT_TRUE(daily.Allow(5, 24 * 60 * 60 + 1)); // the oldest is a day old
}

TEST(DashboardNotifyTests, AnnouncementChatLine) {
	EXPECT_EQ(DashboardNotify::ChatLine("Server restart", "In 5 minutes."), "Server restart: In 5 minutes.");
	EXPECT_EQ(DashboardNotify::ChatLine("Challenge complete!", "Builders: 100 bricks!"), "Challenge complete! Builders: 100 bricks!");
	EXPECT_EQ(DashboardNotify::ChatLine("Ready?", "Go"), "Ready? Go");
	EXPECT_EQ(DashboardNotify::ChatLine("", "Just text"), "Just text");
}
