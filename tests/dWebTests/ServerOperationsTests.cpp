#include <gtest/gtest.h>

#include <cstring>

#include "AnnouncementSchedule.h"
#include "EventSchedule.h"
#include "InstanceLimits.h"
#include "LevelGating.h"
#include "SettingsHistory.h"

namespace {
	// 2026-09-25 (a Friday) at hh:mm UTC
	int64_t At(int hour, int minute, int dayOffset = 0) {
		return (Cron::Detail::DaysFromCivil(2026, 9, 25) + dayOffset) * 86400 + hour * 3600 + minute * 60;
	}

	Cron::Schedule Parse(const std::string& text) {
		std::string error;
		const auto schedule = Cron::Parse(text, error);
		EXPECT_TRUE(schedule.has_value()) << text << ": " << error;
		return schedule.value_or(Cron::Schedule{});
	}

	IServerConfig::Setting Row(const std::string& file, const std::string& name, std::optional<std::string> fileValue, std::optional<std::string> webValue, bool webWins = false) {
		IServerConfig::Setting row;
		row.file = file;
		row.name = name;
		row.fileValue = std::move(fileValue);
		row.fileSource = row.fileValue ? "file" : "";
		row.webValue = std::move(webValue);
		row.webWins = webWins;
		return row;
	}
}

TEST(AnnouncementScheduleTests, CronWithoutLimits) {
	EXPECT_EQ(AnnouncementSchedule::Next(Parse("0 * * * *"), 0, 0, At(10, 15)), At(11, 0));
}

TEST(AnnouncementScheduleTests, WaitsForTheStart) {
	// Cron: the first matching time from the start on, including the start itself
	EXPECT_EQ(AnnouncementSchedule::Next(Parse("0 * * * *"), At(12, 0), 0, At(10, 15)), At(12, 0));
	EXPECT_EQ(AnnouncementSchedule::Next(Parse("30 * * * *"), At(12, 0), 0, At(10, 15)), At(12, 30));
	// An interval fires at the start and then on its beat
	EXPECT_EQ(AnnouncementSchedule::Next(Parse("@every 20m"), At(12, 0), 0, At(10, 15)), At(12, 0));
	EXPECT_EQ(AnnouncementSchedule::Next(Parse("@every 20m"), At(12, 0), 0, At(12, 0)), At(12, 20));
	EXPECT_EQ(AnnouncementSchedule::Next(Parse("@every 20m"), At(12, 0), 0, At(12, 47)), At(13, 0));
}

TEST(AnnouncementScheduleTests, StopsAtTheEnd) {
	EXPECT_EQ(AnnouncementSchedule::Next(Parse("0 * * * *"), 0, At(11, 0), At(10, 15)), At(11, 0));
	EXPECT_FALSE(AnnouncementSchedule::Next(Parse("0 * * * *"), 0, At(10, 59), At(10, 15)).has_value());
	EXPECT_FALSE(AnnouncementSchedule::Next(Parse("@every 1h"), 0, At(11, 0), At(10, 30)).has_value());
}

TEST(AnnouncementScheduleTests, ShortestGap) {
	EXPECT_EQ(AnnouncementSchedule::ShortestGap(Parse("@every 5m"), At(0, 0)), 300);
	EXPECT_EQ(AnnouncementSchedule::ShortestGap(Parse("0,10 * * * *"), At(0, 0)), 600);
	EXPECT_EQ(AnnouncementSchedule::ShortestGap(Parse("@daily"), At(0, 0)), 86400);
}

TEST(AnnouncementScheduleTests, Validate) {
	const auto now = At(10, 0);
	EXPECT_FALSE(AnnouncementSchedule::Validate("", "Double coins weekend!", "0 */2 * * *", 0, 0, {}, now).has_value());
	EXPECT_TRUE(AnnouncementSchedule::Validate("", "", "@hourly", 0, 0, {}, now).has_value());
	EXPECT_TRUE(AnnouncementSchedule::Validate("", std::string(1001, 'x'), "@hourly", 0, 0, {}, now).has_value());
	EXPECT_TRUE(AnnouncementSchedule::Validate(std::string(101, 't'), "Hi", "@hourly", 0, 0, {}, now).has_value());
	EXPECT_TRUE(AnnouncementSchedule::Validate("", "Hi", "not a schedule", 0, 0, {}, now).has_value());
	// More often than once a minute
	EXPECT_TRUE(AnnouncementSchedule::Validate("", "Hi", "@every 30s", 0, 0, {}, now).has_value());
	EXPECT_FALSE(AnnouncementSchedule::Validate("", "Hi", "* * * * *", 0, 0, {}, now).has_value());
	// Dates
	EXPECT_TRUE(AnnouncementSchedule::Validate("", "Hi", "@hourly", At(12, 0), At(11, 0), {}, now).has_value());
	EXPECT_TRUE(AnnouncementSchedule::Validate("", "Hi", "@hourly", 0, At(9, 0), {}, now).has_value());
	// Never fires in the window
	EXPECT_TRUE(AnnouncementSchedule::Validate("", "Hi", "0 * * * *", At(12, 10), At(12, 50), {}, now).has_value());
	EXPECT_FALSE(AnnouncementSchedule::Validate("", "Hi", "0 * * * *", At(12, 10), At(13, 50), {}, now).has_value());
	EXPECT_TRUE(AnnouncementSchedule::Validate("", "Hi", "@hourly", 0, 0, std::vector<uint32_t>(201, 1200), now).has_value());
}

TEST(EventScheduleTests, Slots) {
	EXPECT_EQ(EventSchedule::SlotSetting(3), "event_3");
	EXPECT_EQ(EventSchedule::SlotOf("event_1"), 1);
	EXPECT_EQ(EventSchedule::SlotOf("event_8"), 8);
	EXPECT_EQ(EventSchedule::SlotOf("event_9"), 0);
	EXPECT_EQ(EventSchedule::SlotOf("event_10"), 0);
	EXPECT_EQ(EventSchedule::SlotOf("version_major"), 0);
}

TEST(EventScheduleTests, BusySlotsFromEveryFile) {
	const std::vector<IServerConfig::Setting> rows{
		Row("sharedconfig.ini", "event_1", "Talk_Like_A_Pirate", std::nullopt),
		Row("sharedconfig.ini", "event_2", "", std::nullopt),            // empty in the file: free
		Row("worldconfig.ini", "event_3", "frostburgh", std::nullopt),   // set for the worlds only: still taken
		Row("sharedconfig.ini", "event_4", "", "auramarruins", true),    // set on the web
		Row("sharedconfig.ini", "event_5", "", "", true),                // an empty web value: free
	};
	const auto busy = EventSchedule::BusySlots(rows);
	EXPECT_TRUE(busy[1]);
	EXPECT_FALSE(busy[2]);
	EXPECT_TRUE(busy[3]);
	EXPECT_TRUE(busy[4]);
	EXPECT_FALSE(busy[5]);
	EXPECT_EQ(EventSchedule::FreeSlot(busy), 2);

	std::array<bool, EventSchedule::SLOTS + 1> full{};
	full.fill(true);
	EXPECT_FALSE(EventSchedule::FreeSlot(full).has_value());

	ASSERT_NE(EventSchedule::SlotRow(rows, 4), nullptr);
	EXPECT_EQ(EventSchedule::SlotRow(rows, 4)->webValue, "auramarruins");
	EXPECT_EQ(EventSchedule::SlotRow(rows, 3), nullptr); // only sharedconfig.ini rows are the calendar's
}

TEST(EventScheduleTests, SlotHolding) {
	const std::vector<IServerConfig::Setting> rows{ Row("sharedconfig.ini", "event_2", "oct2011content", std::nullopt), Row("worldconfig.ini", "event_5", std::nullopt, "frostburgh") };
	EXPECT_EQ(EventSchedule::SlotHolding(rows, "oct2011content"), 2);
	EXPECT_EQ(EventSchedule::SlotHolding(rows, "frostburgh"), 5);
	EXPECT_EQ(EventSchedule::SlotHolding(rows, "nope"), 0);
}

TEST(EventScheduleTests, Validate) {
	const auto now = At(10, 0);
	EXPECT_FALSE(EventSchedule::ValidateFeature("oct2011content").has_value());
	EXPECT_TRUE(EventSchedule::ValidateFeature("").has_value());
	EXPECT_TRUE(EventSchedule::ValidateFeature("a=b").has_value());
	EXPECT_FALSE(EventSchedule::ValidateOnce(At(12, 0), At(12, 0, 7), now).has_value());
	EXPECT_TRUE(EventSchedule::ValidateOnce(At(13, 0), At(12, 0), now).has_value());
	EXPECT_TRUE(EventSchedule::ValidateOnce(At(8, 0), At(9, 0), now).has_value());
	EXPECT_TRUE(EventSchedule::ValidateOnce(At(12, 0), At(12, 0, 400), now).has_value());
	EXPECT_TRUE(EventSchedule::ValidateOnce(0, At(12, 0), now).has_value());
	// Started already but still running is fine (it starts at once)
	EXPECT_FALSE(EventSchedule::ValidateOnce(At(8, 0), At(11, 0), now).has_value());
}

TEST(EventScheduleTests, NextState) {
	using eEventState = IServerOperations::eEventState;
	using EventSchedule::NextState;
	const auto end = At(12, 0);
	// On, or a part still to end
	EXPECT_EQ(NextState(eEventState::SCHEDULED, true, end, true, false, false, At(11, 0)), eEventState::ACTIVE);
	EXPECT_EQ(NextState(eEventState::ACTIVE, true, end, false, true, false, At(12, 1)), eEventState::ACTIVE);
	// A once event after its end: ended if it ran, missed if it never did
	EXPECT_EQ(NextState(eEventState::ACTIVE, true, end, false, false, false, At(12, 1)), eEventState::ENDED);
	EXPECT_EQ(NextState(eEventState::ENDED, true, end, false, false, false, At(13, 0)), eEventState::ENDED);
	EXPECT_EQ(NextState(eEventState::SCHEDULED, true, end, false, false, false, At(12, 1)), eEventState::MISSED);
	// Cancelled stays while it is off; switched back on it waits again
	EXPECT_EQ(NextState(eEventState::CANCELLED, true, end, false, false, true, At(11, 0)), eEventState::CANCELLED);
	EXPECT_EQ(NextState(eEventState::CANCELLED, true, end, false, false, false, At(11, 0)), eEventState::SCHEDULED);
	// A recurring event between its times
	EXPECT_EQ(NextState(eEventState::ACTIVE, false, 0, false, false, false, At(11, 0)), eEventState::SCHEDULED);
	EXPECT_EQ(NextState(eEventState::SCHEDULED, false, 0, false, false, true, At(11, 0)), eEventState::SCHEDULED);
}

TEST(SettingsHistoryTests, Changes) {
	using SettingsHistory::Changes;
	EXPECT_FALSE(Changes(std::nullopt, false, std::nullopt, false));
	EXPECT_FALSE(Changes(std::nullopt, true, std::nullopt, false)); // "wins" means nothing without a value
	EXPECT_FALSE(Changes("5", true, "5", true));
	EXPECT_TRUE(Changes("5", false, "5", true));
	EXPECT_TRUE(Changes("5", false, "6", false));
	EXPECT_TRUE(Changes(std::nullopt, false, "", false)); // an empty value is still a value
	EXPECT_TRUE(Changes("5", false, std::nullopt, false));
}

TEST(SettingsHistoryTests, Revert) {
	IServerConfig::SettingChange change;
	change.file = "worldconfig.ini";
	change.name = "max_clients";
	change.oldValue = "20";
	change.oldWebWins = true;
	change.newValue = "40";
	change.newWebWins = true;

	std::string error;
	auto body = SettingsHistory::RevertBody(change, error);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ((*body)["file"], "worldconfig.ini");
	EXPECT_EQ((*body)["name"], "max_clients");
	EXPECT_EQ((*body)["value"], "20");
	EXPECT_EQ((*body)["webWins"], true);

	// Undoing the first value set clears it again
	change.oldValue.reset();
	change.oldWebWins = false;
	body = SettingsHistory::RevertBody(change, error);
	ASSERT_TRUE(body.has_value());
	EXPECT_TRUE((*body)["value"].is_null());
	EXPECT_EQ((*body)["webWins"], false);

	change.secret = true;
	EXPECT_FALSE(SettingsHistory::RevertBody(change, error).has_value());
	EXPECT_FALSE(error.empty());
}

TEST(SettingsHistoryTests, StillCurrent) {
	IServerConfig::SettingChange change;
	change.newValue = "40";
	change.newWebWins = false;
	EXPECT_TRUE(SettingsHistory::StillCurrent(change, "40", false));
	EXPECT_FALSE(SettingsHistory::StillCurrent(change, "41", false));
	EXPECT_FALSE(SettingsHistory::StillCurrent(change, "40", true));
	change.newValue.reset(); // cleared
	EXPECT_TRUE(SettingsHistory::StillCurrent(change, std::nullopt, false));
	EXPECT_FALSE(SettingsHistory::StillCurrent(change, "1", false));
}

TEST(InstanceLimitsTests, Effective) {
	const InstanceLimits::Caps client{ 8, 12 };
	EXPECT_EQ(InstanceLimits::Effective(std::nullopt, std::nullopt, client).soft, 8u);
	EXPECT_EQ(InstanceLimits::Effective(std::nullopt, std::nullopt, client).hard, 12u);
	EXPECT_EQ(InstanceLimits::Effective(20, 30, client).soft, 20u);
	EXPECT_EQ(InstanceLimits::Effective(20, 30, client).hard, 30u);
	// A lower hard cap pulls the client's soft cap down with it
	EXPECT_EQ(InstanceLimits::Effective(std::nullopt, 5, client).soft, 5u);
}

TEST(InstanceLimitsTests, Validate) {
	const InstanceLimits::Caps client{ 8, 12 };
	EXPECT_FALSE(InstanceLimits::Validate(1100, 20, 30, 1, client).has_value());
	EXPECT_FALSE(InstanceLimits::Validate(1100, std::nullopt, std::nullopt, 0, client).has_value());
	EXPECT_TRUE(InstanceLimits::Validate(0, 20, 30, 0, client).has_value());
	EXPECT_TRUE(InstanceLimits::Validate(1100, 0, 30, 0, client).has_value());
	EXPECT_TRUE(InstanceLimits::Validate(1100, 20, 501, 0, client).has_value());
	EXPECT_TRUE(InstanceLimits::Validate(1100, 20, std::nullopt, 0, client).has_value()); // above the client's hard cap of 12
	EXPECT_TRUE(InstanceLimits::Validate(1100, 20, 10, 0, client).has_value());
	EXPECT_TRUE(InstanceLimits::Validate(1100, std::nullopt, std::nullopt, 6, client).has_value());
}

namespace {
	struct LvlWriter {
		std::string data;
		template<typename T> LvlWriter& Put(T value) { data.append(reinterpret_cast<const char*>(&value), sizeof(T)); return *this; }
		template<typename T> void At(size_t pos, T value) { std::memcpy(data.data() + pos, &value, sizeof(T)); }
		size_t Chunk(uint32_t id) {
			const auto start = data.size();
			Put<uint32_t>(0x4B4E4843).Put<uint32_t>(id).Put<uint16_t>(1).Put<uint16_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
			At<uint32_t>(start + 16, static_cast<uint32_t>(data.size()));
			return start;
		}
		void End(size_t chunk) { At<uint32_t>(chunk + 12, static_cast<uint32_t>(data.size() - chunk)); }
		void Object(uint32_t lot, const std::string& settings) {
			Put<uint64_t>(1).Put<uint32_t>(lot).Put<uint32_t>(0).Put<uint32_t>(0);
			Put(0.0f).Put(0.0f).Put(0.0f).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put(1.0f);
			Put<uint32_t>(static_cast<uint32_t>(settings.size()));
			for (char c : settings) Put<uint16_t>(static_cast<uint16_t>(c));
			Put<uint32_t>(0);
		}
	};
}

TEST(LevelGatingTests, CountsGatedObjects) {
	LvlWriter w;
	auto info = w.Chunk(1000);
	w.Put<uint32_t>(41).Put<uint32_t>(1).Put<uint32_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
	w.End(info);
	auto objects = w.Chunk(2001);
	w.Put<uint32_t>(5);
	w.Object(1000, "gatingOnFeature=13:oct2011content");
	w.Object(1001, "custom_config_names=0:\ngatingOnFeature=13:oct2011content\r");
	w.Object(1002, "gatingOnFeature=13:auramarruins");
	w.Object(1003, "gatingOnFeature=13:oct2011content\nloadOnClientOnly=7:1"); // the world never loads it
	w.Object(1004, "spawntemplate=1:6010");
	w.End(objects);

	const auto features = LevelGating::ReadGatedFeatures(w.data);
	ASSERT_EQ(features.size(), 2u);
	EXPECT_EQ(features.at("oct2011content"), 2u);
	EXPECT_EQ(features.at("auramarruins"), 1u);
	EXPECT_TRUE(LevelGating::ReadGatedFeatures("").empty());
}
