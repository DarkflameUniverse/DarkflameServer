#include "HotPropertySlots.h"

#include <gtest/gtest.h>

using namespace HotPropertySlots;

namespace {
	// A cut of the 1.10.64 CDClient: test and unused rows share the spawn names; only the live worlds have entrances
	const std::vector<TemplateRow> TEMPLATES{
		{ 25218, 58001, "AGSmallProperty" }, { 25219, 521, "AGSmallProperty" }, { 25220, 162, "GFSmallProperty" },
		{ 25221, 63, "NSSmallProperty" }, { 25166, 1150, "AGSmallProperty" }, { 25168, 1151, "AGMedProperty" },
		{ 25188, 1250, "NSSmallProperty" }, { 25189, 1251, "NSMedProperty" }, { 25191, 1350, "GFSmallProperty" },
		{ 25194, 1450, "FVSmallProperty" }, { 25208, 45, "FVSmallProperty" }, { 25216, 542, "TestPath" },
	};
	const std::vector<EntranceRow> ENTRANCES{
		{ 1150, "AGSmallProperty" }, { 1150, "AGSmallProperty" }, { 1151, "AGMedProperty" }, { 1250, "NSSmallProperty" },
		{ 1251, "NSMedProperty" }, { 1350, "GFSmallProperty" }, { 1450, "FVSmallProperty" },
	};
}

// The four slots are the template ids live sent: 25166, 25188, 25191, 25194, in that order
TEST(HotPropertySlotsTests, ResolvesTheFourLiveSlots) {
	const auto slots = ResolveSlots(TEMPLATES, ENTRANCES);
	const std::vector<Slot> expected{
		{ 25166, 1150, "AGSmallProperty" }, { 25188, 1250, "NSSmallProperty" }, { 25191, 1350, "GFSmallProperty" }, { 25194, 1450, "FVSmallProperty" },
	};
	EXPECT_EQ(slots, expected);
}

TEST(HotPropertySlotsTests, WorldsTheNewsScreenCannotShowHaveNoSlot) {
	for (const auto& slot : ResolveSlots(TEMPLATES, ENTRANCES)) {
		EXPECT_NE(slot.mapId, 1151u);
		EXPECT_NE(slot.mapId, 1251u);
	}
}

TEST(HotPropertySlotsTests, SlotWithoutEntranceIsLeftOut) {
	const std::vector<EntranceRow> entrances{ { 1250, "NSSmallProperty" }, { 1450, "FVSmallProperty" } };
	const auto slots = ResolveSlots(TEMPLATES, entrances);
	ASSERT_EQ(slots.size(), 2u);
	EXPECT_EQ(slots[0].templateId, 25188u);
	EXPECT_EQ(slots[1].templateId, 25194u);
	EXPECT_TRUE(ResolveSlots(TEMPLATES, {}).empty());
}

TEST(HotPropertySlotsTests, LowestTemplateIdWinsWhenSeveralMatch) {
	const std::vector<TemplateRow> templates{ { 30000, 1150, "AGSmallProperty" }, { 25166, 1150, "AGSmallProperty" } };
	const auto slots = ResolveSlots(templates, ENTRANCES);
	ASSERT_EQ(slots.size(), 1u);
	EXPECT_EQ(slots[0].templateId, 25166u);
}

TEST(HotPropertySlotsTests, OnlyApprovedPublicPropertiesOfTheSlotsWorld) {
	EXPECT_TRUE(Featurable(1, 2, 1150, 1150));
	EXPECT_FALSE(Featurable(0, 2, 1150, 1150)); // awaiting review
	EXPECT_FALSE(Featurable(2, 2, 1150, 1150));
	EXPECT_FALSE(Featurable(1, 1, 1150, 1150)); // friends only
	EXPECT_FALSE(Featurable(1, 0, 1150, 1150)); // private
	EXPECT_FALSE(Featurable(1, 2, 1151, 1150)); // another world
}

// news.gfx always walks four entries and reuses the previous slot for a missing one
TEST(HotPropertySlotsTests, NewsOrderPadsWithTheLastEntry) {
	EXPECT_TRUE(NewsOrder(0).empty());
	EXPECT_EQ(NewsOrder(1), (std::vector<size_t>{ 0, 0, 0, 0 }));
	EXPECT_EQ(NewsOrder(2), (std::vector<size_t>{ 0, 1, 1, 1 }));
	EXPECT_EQ(NewsOrder(3), (std::vector<size_t>{ 0, 1, 2, 2 }));
	EXPECT_EQ(NewsOrder(4), (std::vector<size_t>{ 0, 1, 2, 3 }));
}

TEST(HotPropertySlotsTests, Modes) {
	for (const auto mode : { eMode::AUTO, eMode::PICKED, eMode::EMPTY }) {
		EXPECT_EQ(ParseMode(ModeName(mode)), mode);
		EXPECT_EQ(ModeFromInt(static_cast<int64_t>(mode)), mode);
	}
	EXPECT_EQ(ParseMode("bogus"), std::nullopt);
	EXPECT_EQ(ModeFromInt(7), eMode::AUTO);
	// Stored values the database keeps; never renumber
	EXPECT_EQ(static_cast<int>(eMode::AUTO), 0);
	EXPECT_EQ(static_cast<int>(eMode::PICKED), 1);
	EXPECT_EQ(static_cast<int>(eMode::EMPTY), 2);
}

// Every property world players can launch to, small and medium; test and unused maps are left out
TEST(HotPropertySlotsTests, PropertyWorldsAreTheEnteredTemplateMaps) {
	EXPECT_EQ(PropertyWorlds(TEMPLATES, ENTRANCES), (std::vector<uint32_t>{ 1150, 1151, 1250, 1251, 1350, 1450 }));
	EXPECT_TRUE(PropertyWorlds(TEMPLATES, {}).empty());
}

TEST(HotPropertySlotsTests, LocationDefaultsToTheSlotsWorld) {
	const auto worlds = PropertyWorlds(TEMPLATES, ENTRANCES);
	const Slot blockYard{ 25166, 1150, "AGSmallProperty" };
	EXPECT_EQ(Location(0, blockYard, worlds), 1150u);    // rows from before locations
	EXPECT_EQ(Location(1251, blockYard, worlds), 1251u); // Nimbus Isle
	EXPECT_EQ(Location(58001, blockYard, worlds), 1150u); // not a property world (any more)
}

namespace {
	// Approved public properties: 1xx on Block Yard (1150), 2xx on Avant Grove (1151), 3xx on Nimbus Rock (1250)
	const std::vector<Candidate> CANDIDATES{
		{ 101, 1150, 500 }, { 102, 1150, 400 }, { 103, 1150, 300 }, { 201, 1151, 900 }, { 202, 1151, 50 }, { 301, 1250, 700 },
	};

	std::vector<std::optional<int64_t>> Shown(const std::vector<Showing>& showing) {
		std::vector<std::optional<int64_t>> ids;
		for (const auto& slot : showing) ids.push_back(slot.propertyId);
		return ids;
	}

	using Ids = std::vector<std::optional<int64_t>>;
}

TEST(HotPropertySlotsTests, AutoShowsEachLocationsBest) {
	const std::vector<Choice> choices{ { eMode::AUTO, 1150 }, { eMode::AUTO, 1250 }, { eMode::AUTO, 1350 }, { eMode::AUTO, 1151 } };
	EXPECT_EQ(Shown(Resolve(choices, false, CANDIDATES)), (Ids{ 101, 301, std::nullopt, 201 }));
}

TEST(HotPropertySlotsTests, AutosOnOneLocationShowItsFirstAndSecond) {
	const std::vector<Choice> choices{ { eMode::AUTO, 1150 }, { eMode::AUTO, 1150 }, { eMode::AUTO, 1150 }, { eMode::AUTO, 1150 } };
	EXPECT_EQ(Shown(Resolve(choices, false, CANDIDATES)), (Ids{ 101, 102, 103, std::nullopt }));
}

TEST(HotPropertySlotsTests, PicksComeFirstAndAutosSkipThem) {
	// The first slot's auto would be 101, but a later slot picked it
	const std::vector<Choice> choices{ { eMode::AUTO, 1150 }, { eMode::PICKED, 1150, 101 }, { eMode::EMPTY, 1350 }, { eMode::AUTO, 1150 } };
	const auto showing = Resolve(choices, false, CANDIDATES);
	EXPECT_EQ(Shown(showing), (Ids{ 102, 101, std::nullopt, 103 }));
	EXPECT_FALSE(showing[1].pickFellBack);
}

TEST(HotPropertySlotsTests, APropertyPickedTwiceIsShownOnce) {
	const std::vector<Choice> choices{ { eMode::PICKED, 1150, 103 }, { eMode::PICKED, 1150, 103 }, { eMode::AUTO, 1250 }, { eMode::AUTO, 1250 } };
	const auto showing = Resolve(choices, false, CANDIDATES);
	EXPECT_EQ(Shown(showing), (Ids{ 103, 101, 301, std::nullopt }));
	EXPECT_FALSE(showing[0].pickFellBack);
	EXPECT_TRUE(showing[1].pickFellBack);
}

TEST(HotPropertySlotsTests, AnUnavailablePickFallsBackToAuto) {
	// 999 is no candidate (no longer approved or public); 301 is, but not of the slot's location
	const std::vector<Choice> choices{ { eMode::PICKED, 1150, 999 }, { eMode::PICKED, 1150, 301 }, { eMode::AUTO, 1150 }, { eMode::EMPTY, 1450 } };
	const auto showing = Resolve(choices, false, CANDIDATES);
	EXPECT_EQ(Shown(showing), (Ids{ 101, 102, 103, std::nullopt }));
	EXPECT_TRUE(showing[0].pickFellBack);
	EXPECT_TRUE(showing[1].pickFellBack);
	EXPECT_FALSE(showing[2].pickFellBack);
}

TEST(HotPropertySlotsTests, APickOfAnotherWorld) {
	const std::vector<Choice> choices{ { eMode::PICKED, 1151, 202 }, { eMode::AUTO, 1151 }, { eMode::AUTO, 1151 }, { eMode::AUTO, 1250 } };
	EXPECT_EQ(Shown(Resolve(choices, false, CANDIDATES)), (Ids{ 202, 201, std::nullopt, 301 }));
}

TEST(HotPropertySlotsTests, FullAutoShowsTheTopFourAcrossEveryWorld) {
	// The choices don't matter
	const std::vector<Choice> choices{ { eMode::EMPTY, 1150 }, { eMode::PICKED, 1150, 103 }, { eMode::AUTO, 1350 }, { eMode::AUTO, 1450 } };
	const auto showing = Resolve(choices, true, CANDIDATES);
	EXPECT_EQ(Shown(showing), (Ids{ 201, 301, 101, 102 }));
	for (const auto& slot : showing) EXPECT_FALSE(slot.pickFellBack);
}

TEST(HotPropertySlotsTests, FullAutoWithFewerPropertiesThanSlots) {
	const std::vector<Choice> choices(4);
	EXPECT_EQ(Shown(Resolve(choices, true, { { 202, 1151, 50 }, { 301, 1250, 700 } })), (Ids{ 301, 202, std::nullopt, std::nullopt }));
	EXPECT_EQ(Shown(Resolve(choices, true, {})), (Ids(4, std::nullopt)));
}

TEST(HotPropertySlotsTests, ACandidateListedTwiceCountsOnce) {
	// A picked property is added to the candidates even when a world's list already has it
	auto candidates = CANDIDATES;
	candidates.push_back({ 101, 1150, 500 });
	const std::vector<Choice> choices{ { eMode::AUTO, 1150 }, { eMode::AUTO, 1150 }, { eMode::AUTO, 1150 }, { eMode::AUTO, 1150 } };
	EXPECT_EQ(Shown(Resolve(choices, false, candidates)), (Ids{ 101, 102, 103, std::nullopt }));
}

TEST(HotPropertySlotsTests, CandidateWorlds) {
	const std::vector<Choice> choices{ { eMode::AUTO, 1150 }, { eMode::PICKED, 1151, 202 }, { eMode::EMPTY, 1350 }, { eMode::AUTO, 1150 } };
	EXPECT_EQ(CandidateWorlds(choices, false), (std::vector<uint32_t>{ 1150, 1151 }));
	EXPECT_EQ(CandidateWorlds(choices, true), (std::vector<uint32_t>{ 0 }));
}
