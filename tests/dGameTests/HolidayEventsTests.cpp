// EventGating (a server-only table) as GetHolidayEvent answered it: an event runs while its dates include the time;
// a server can also name it in event_1..event_8.
#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDEventGatingTable.h"
#include "HolidayEvents.h"

#include <array>
#include <string_view>

#include <gtest/gtest.h>

class HolidayEventsTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		// Rows as in the 1.10.64 CDClient
		auto& rows = CDClientManager::GetEntriesMutable<CDEventGatingTable>();
		rows.push_back({ "pirateDay", 1316390400, 1316563199 });
		rows.push_back({ "buildNexusTower", 1268092800, 1299801600 });
	}

	void TearDown() override {
		CDClientManager::GetEntriesMutable<CDEventGatingTable>().clear();
		TearDownDependencies();
	}
};

TEST_F(HolidayEventsTests, DatesIncludeBothEnds) {
	const auto* const table = CDClientManager::GetTable<CDEventGatingTable>();
	EXPECT_FALSE(table->IsEventActive("pirateDay", 1316390399));
	EXPECT_TRUE(table->IsEventActive("pirateDay", 1316390400)); // 2011-09-19 00:00 UTC
	EXPECT_TRUE(table->IsEventActive("pirateDay", 1316563199)); // 2011-09-20 23:59:59 UTC
	EXPECT_FALSE(table->IsEventActive("pirateDay", 1316563200));
	EXPECT_FALSE(table->IsEventActive("noSuchEvent", 1316390400));
}

TEST_F(HolidayEventsTests, ServerSettingTurnsAnEventOn) {
	constexpr int64_t now = 1790000000; // 2026: no live dates match
	const std::array<std::string_view, 8> none{};
	EXPECT_FALSE(HolidayEvents::IsActive("pirateDay", now, none.data(), none.size()));

	const std::array<std::string_view, 8> pirates{ "Talk_Like_A_Pirate", "pirateDay" };
	EXPECT_TRUE(HolidayEvents::IsActive("pirateDay", now, pirates.data(), pirates.size()));
	EXPECT_FALSE(HolidayEvents::IsActive("buildNexusTower", now, pirates.data(), pirates.size()));
	EXPECT_FALSE(HolidayEvents::IsActive("", now, none.data(), none.size())); // empty event_N settings don't match
	EXPECT_TRUE(HolidayEvents::IsActive("buildNexusTower", 1268092800, none.data(), none.size()));
}
