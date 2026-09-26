#include <gtest/gtest.h>

#include "InventoryRestore.h"
#include "MissionXml.h"
#include "CharacterProgress.h"

using namespace InventoryRestore;

namespace {
	const Missing* Find(const Diff& diff, LOT lot) {
		for (const auto& entry : diff.missing) if (entry.lot == lot) return &entry;
		return nullptr;
	}
}

TEST(InventoryRestoreTests, HeldSkipsProxiesAndVendorItems) {
	const auto items = Held(R"(<obj><inv><items><in t="0"><i l="10" id="1" c="2"/><i l="11" id="2" c="1" parent="1"/></in>)"
		R"(<in t="11"><i l="12" id="3" c="1"/></in><in t="1"><i l="13" id="4" c="5"/></in></items></inv></obj>)");
	ASSERT_EQ(items.size(), 2u);
	EXPECT_EQ(items[0].lot, 10);
	EXPECT_EQ(items[1].inventory, 1u);
	EXPECT_EQ(items[1].count, 5u);
}

TEST(InventoryRestoreTests, MovedAndSplitItemsAreNotLost) {
	// Item 1 moved to the vault, item 2's stack split into a new id
	const std::vector<Item> then{ { 1, 10, 1, 0 }, { 2, 20, 10, 0 } };
	const std::vector<Item> now{ { 1, 10, 1, 1 }, { 2, 20, 4, 0 }, { 3, 20, 6, 0 } };
	const auto diff = Compare(then, now, {}, {}, {});
	EXPECT_TRUE(diff.missing.empty());
	ASSERT_EQ(diff.changes.size(), 2u); // lot 10 left items and arrived in the vault; lot 20 in items is unchanged overall
	EXPECT_EQ(diff.changes[0].inventory, 0u);
	EXPECT_EQ(diff.changes[0].then, 1u);
	EXPECT_EQ(diff.changes[0].now, 0u);
}

TEST(InventoryRestoreTests, LostItemsAndWhatIsAlreadyAccountedFor) {
	const std::vector<Item> then{ { 1, 10, 1, 0 }, { 2, 20, 50, 0 }, { 3, 30, 1, 0 }, { 4, 40, 1, 0 }, { 5, 50, 3, 0 } };
	const std::vector<Item> now{ { 2, 20, 20, 0 } };
	// Lot 30's object was traded to someone; one lot 40 waits in the mailbox; object 5 is in the mailbox with its id
	const auto diff = Compare(then, now, { { 40, 1 }, { 50, 3 } }, { 5 }, { { 3, 1 } });
	const auto* sword = Find(diff, 10);
	ASSERT_TRUE(sword);
	EXPECT_EQ(sword->restorable, 1u);
	ASSERT_EQ(sword->lost.size(), 1u);
	EXPECT_EQ(sword->lost[0].id, 1);
	const auto* bricks = Find(diff, 20);
	ASSERT_TRUE(bricks);
	EXPECT_EQ(bricks->restorable, 30u);
	EXPECT_TRUE(bricks->lost.empty()); // the object is still held, only fewer
	EXPECT_EQ(Find(diff, 30)->elsewhere, 1u);
	EXPECT_EQ(Find(diff, 30)->restorable, 0u);
	EXPECT_EQ(Find(diff, 40)->restorable, 0u);
	EXPECT_EQ(Find(diff, 50)->restorable, 0u);
	EXPECT_TRUE(Find(diff, 50)->lost.empty());
}

TEST(InventoryRestoreTests, PlanUsesOriginalIdsAndSplitsLargeCounts) {
	Missing missing;
	missing.lot = 7;
	missing.restorable = 2500;
	missing.lost = { { 100, 400, 0 }, { 101, 5000, 0 } };
	const auto mails = Plan(missing, 5000);
	ASSERT_EQ(mails.size(), 4u);
	EXPECT_EQ(mails[0].originalId, 100);
	EXPECT_EQ(mails[0].count, 400u);
	uint64_t total = 0;
	for (const auto& mail : mails) { total += mail.count; EXPECT_LE(mail.count, MAX_MAIL_COUNT); }
	EXPECT_EQ(total, 2500u); // capped at what is restorable
	EXPECT_EQ(mails[1].originalId, LWOOBJID_EMPTY);
	EXPECT_TRUE(Plan(missing, 0).empty());
	missing.restorable = 0;
	EXPECT_TRUE(Plan(missing, 10).empty());
}

namespace {
	const std::string MISSIONS = R"(<obj v="1"><char cc="1"/><mis><done><m state="8" id="1" cct="1" cts="100"/><m state="8" id="3" cct="2" cts="200"/></done>)"
		R"(<cur><m state="2" o="4" id="2"><sv v="2"/><sv v="1"/></m><m state="2" id="5"><sv v="2"/><sv v="300"/><sv v="301"/><sv v="1"/></m>)"
		R"(<m state="10" o="7" id="3"><sv v="0"/></m></cur></mis></obj>)";

	const MissionXml::Definition MISSION{ true, false, { eMissionTaskType::SMASH, eMissionTaskType::TALK_TO_NPC } };
	const MissionXml::Definition COLLECT{ false, false, { eMissionTaskType::COLLECTION, eMissionTaskType::SCRIPT } };
	const MissionXml::Definition DAILY{ true, true, { eMissionTaskType::SMASH } };

	const MissionXml::Definition* Lookup(uint32_t id) {
		if (id == 5) return &COLLECT;
		if (id == 3) return &DAILY;
		return &MISSION;
	}

	std::map<uint32_t, MissionXml::Entry> Changed(uint32_t id, const MissionXml::Definition& definition, MissionXml::eChange change, std::string& error) {
		const auto xml = MissionXml::Change(MISSIONS, id, definition, change, 1000, error);
		return xml ? MissionXml::Read(*xml, Lookup) : std::map<uint32_t, MissionXml::Entry>{};
	}
}

TEST(MissionXmlTests, Reads) {
	const auto entries = MissionXml::Read(MISSIONS, Lookup);
	ASSERT_EQ(entries.size(), 4u);
	EXPECT_TRUE(entries.at(1).Done());
	EXPECT_FALSE(entries.at(1).current);
	EXPECT_EQ(entries.at(2).tasks.size(), 2u);
	EXPECT_EQ(entries.at(2).order, 4u);
	EXPECT_FALSE(entries.at(2).Done());
	// Collected ids follow a collection task's progress
	ASSERT_EQ(entries.at(5).tasks.size(), 2u);
	EXPECT_EQ(entries.at(5).tasks[0].uniques, (std::vector<uint32_t>{ 300, 301 }));
	EXPECT_EQ(entries.at(5).tasks[1].progress, 1u);
	// A daily done twice and taken again
	EXPECT_TRUE(entries.at(3).Done());
	EXPECT_TRUE(entries.at(3).current);
	EXPECT_EQ(entries.at(3).state, eMissionState::COMPLETE_ACTIVE);
	EXPECT_EQ(entries.at(3).completions, 2u);
	EXPECT_TRUE(MissionXml::Read("<obj/>", Lookup).empty());
}

TEST(MissionXmlTests, CompleteMovesToDone) {
	std::string error;
	auto entries = Changed(2, MISSION, MissionXml::eChange::COMPLETE, error);
	ASSERT_TRUE(error.empty()) << error;
	EXPECT_FALSE(entries.at(2).current);
	EXPECT_EQ(entries.at(2).completions, 1u);
	EXPECT_EQ(entries.at(2).completedAt, 1000u);
	EXPECT_EQ(entries.at(2).state, eMissionState::COMPLETE);
	// A mission it never had is taken and completed at once
	entries = Changed(9, MISSION, MissionXml::eChange::COMPLETE, error);
	EXPECT_TRUE(entries.at(9).Done());
	// Done already, and not repeatable
	EXPECT_FALSE(MissionXml::Change(MISSIONS, 1, MISSION, MissionXml::eChange::COMPLETE, 1000, error));
	EXPECT_FALSE(error.empty());
	// A repeatable one counts another completion
	error.clear();
	entries = Changed(3, DAILY, MissionXml::eChange::COMPLETE, error);
	EXPECT_EQ(entries.at(3).completions, 3u);
	EXPECT_FALSE(entries.at(3).current);
}

TEST(MissionXmlTests, ResetForgetsIt) {
	std::string error;
	auto entries = Changed(3, DAILY, MissionXml::eChange::RESET, error);
	EXPECT_FALSE(entries.contains(3));
	EXPECT_EQ(entries.size(), 3u);
	EXPECT_FALSE(MissionXml::Change(MISSIONS, 42, MISSION, MissionXml::eChange::RESET, 1000, error));
}

TEST(MissionXmlTests, AcceptAddsToTheJournal) {
	std::string error;
	auto entries = Changed(9, MISSION, MissionXml::eChange::ACCEPT, error);
	ASSERT_TRUE(entries.contains(9)) << error;
	EXPECT_TRUE(entries.at(9).current);
	EXPECT_EQ(entries.at(9).state, eMissionState::ACTIVE);
	EXPECT_EQ(entries.at(9).order, 8u); // after the highest order in the journal
	EXPECT_EQ(entries.at(9).tasks.size(), 2u);
	// A repeatable one done before is taken again
	entries = Changed(1, DAILY, MissionXml::eChange::ACCEPT, error);
	EXPECT_EQ(entries.at(1).state, eMissionState::COMPLETE_ACTIVE);
	// Something it has (and can't repeat) stays as it is
	EXPECT_FALSE(MissionXml::Change(MISSIONS, 2, MISSION, MissionXml::eChange::ACCEPT, 1000, error));
	EXPECT_FALSE(MissionXml::Change(MISSIONS, 1, MISSION, MissionXml::eChange::ACCEPT, 1000, error));
}

TEST(CharacterProgressTests, Standing) {
	const std::vector<uint32_t> counts{ 1, 2, 2, 5, 10 };
	const auto standing = CharacterProgress::Compare(counts, 5);
	EXPECT_DOUBLE_EQ(standing.average, 4.0);
	EXPECT_DOUBLE_EQ(standing.percentile, 60.0);
	EXPECT_DOUBLE_EQ(CharacterProgress::Compare(counts, 0).percentile, 0.0);
	EXPECT_DOUBLE_EQ(CharacterProgress::Compare({}, 3).average, 0.0);
}
