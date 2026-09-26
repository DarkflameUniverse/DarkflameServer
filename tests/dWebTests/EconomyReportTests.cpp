#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

#include "EconomyScan.h"
#include "TerrainMap.h"

using namespace EconomyScan;

TEST(EconomyScanTests, AttributeMatchesWholeNames) {
	constexpr std::string_view tag = R"(<i l="1727" id="1152921510" c="3" sk="0" parent="0">)";
	EXPECT_EQ(Attribute(tag, "l"), "1727");
	EXPECT_EQ(Attribute(tag, "id"), "1152921510");
	EXPECT_EQ(Attribute(tag, "c"), "3");
	EXPECT_EQ(Attribute(tag, "k"), ""); // "sk" must not match "k"
	EXPECT_EQ(Attribute(tag, "missing"), "");
}

TEST(EconomyScanTests, ReadsItemsPerInventory) {
	const std::string xml =
		R"(<obj v="1"><char cc="100"/><inv><bag><b t="0" m="20"/></bag><items>)"
		R"(<in t="0"><i l="1727" id="11" s="0" c="1" b="1" eq="1" sk="0"/><i l="3040" id="12" s="1" c="50" b="0" eq="0" sk="0"/></in>)"
		R"(<in t="5"><i l="6416" id="13" s="0" c="1" b="0" eq="0" sk="0"/><i l="7000" id="14" s="1" c="1" parent="13"/></in>)"
		R"(</items></inv><mis><cur><m id="1"/></cur></mis></obj>)";
	std::vector<InventoryItem> items;
	ForEachInventoryItem(xml, [&](const InventoryItem& item) { items.push_back(item); });
	ASSERT_EQ(items.size(), 4);
	EXPECT_EQ(items[0].inventoryType, 0);
	EXPECT_EQ(items[0].lot, 1727);
	EXPECT_EQ(items[0].id, 11);
	EXPECT_EQ(items[1].count, 50);
	EXPECT_EQ(items[2].inventoryType, 5);
	EXPECT_FALSE(items[2].isProxy);
	EXPECT_TRUE(items[3].isProxy);
}

TEST(EconomyScanTests, IgnoresXmlWithoutItems) {
	int visited = 0;
	ForEachInventoryItem(R"(<obj><m id="5" l="3"/></obj>)", [&](const InventoryItem&) { visited++; });
	EXPECT_EQ(visited, 0);
}

TEST(EconomyScanTests, FindsDuplicatedIds) {
	DuplicateFinder finder;
	finder.Add(1, { Location::eKind::CHARACTER, 100, 0, 0, 5, 1 });
	finder.Add(2, { Location::eKind::CHARACTER, 100, 0, 0, 6, 1 });
	finder.Add(1, { Location::eKind::CHARACTER, 200, 0, 1, 5, 1 });
	finder.Add(1, { Location::eKind::MAIL, 300, 77, 0, 5, 1 });
	finder.Add(3, { Location::eKind::CHARACTER, 100, 0, 0, 7, 1 });
	finder.Add(3, { Location::eKind::CHARACTER, 100, 0, 1, 7, 1 });
	finder.Add(0, { Location::eKind::MAIL, 300, 78, 0, 5, 1 }); // mail without an object id is not an item id
	finder.Add(0, { Location::eKind::MAIL, 300, 79, 0, 5, 1 });

	EXPECT_EQ(finder.ItemsSeen(), 3);
	const auto duplicates = finder.Duplicates();
	ASSERT_EQ(duplicates.size(), 2);
	EXPECT_EQ(duplicates[0].first, 1); // most copies first
	ASSERT_EQ(duplicates[0].second.size(), 3);
	EXPECT_EQ(duplicates[0].second[0].ownerId, 100);
	EXPECT_EQ(duplicates[0].second[2].mailId, 77);
	EXPECT_EQ(duplicates[1].first, 3);
}

TEST(EconomyScanTests, ClassifiesCopiesByLot) {
	// The same LOT twice is a duplicate
	auto same = Classify({ { Location::eKind::CHARACTER, 100, 0, 0, 5, 1 }, { Location::eKind::CHARACTER, 200, 0, 0, 5, 1 } });
	EXPECT_FALSE(same.collision);
	ASSERT_EQ(same.duplicates.size(), 1);
	EXPECT_EQ(same.duplicates[0].size(), 2);

	// Different LOTs sharing an id (old data) are a collision, not a dupe
	auto different = Classify({ { Location::eKind::CHARACTER, 100, 0, 0, 5182, 1 }, { Location::eKind::CHARACTER, 200, 0, 0, 6093, 1 } });
	EXPECT_TRUE(different.collision);
	EXPECT_TRUE(different.duplicates.empty());

	// Both: only the LOT found twice counts as duplicated
	auto both = Classify({ { Location::eKind::CHARACTER, 100, 0, 0, 5, 1 }, { Location::eKind::CHARACTER, 200, 0, 0, 6, 1 },
		{ Location::eKind::MAIL, 300, 7, 0, 5, 1 } });
	EXPECT_TRUE(both.collision);
	ASSERT_EQ(both.duplicates.size(), 1);
	ASSERT_EQ(both.duplicates[0].size(), 2);
	EXPECT_EQ(both.duplicates[0][0].lot, 5);
	EXPECT_EQ(both.duplicates[0][1].mailId, 7);
}

TEST(EconomyScanTests, ReadsCharacterVersion) {
	EXPECT_EQ(CharacterVersion(R"(<obj v="1"><lvl l="12" cv="5" sb="500"/><inv/></obj>)"), 5u);
	EXPECT_EQ(CharacterVersion(R"(<obj><lvl l="3"/></obj>)"), 0u); // no cv: the game treats it as RELEASE
	EXPECT_EQ(CharacterVersion(R"(<obj><inv/></obj>)"), 0u);
	// cv must be a whole attribute name
	EXPECT_EQ(CharacterVersion(R"(<obj><lvl l="3" xcv="4" cv="9"/></obj>)"), 9u);
}

TEST(EconomyScanTests, NewItemIdsUntilPersistentIdVersion) {
	// The PET_IDS step regenerates item ids, so every save before INVENTORY_PERSISTENT_IDS gets them
	for (uint32_t version = 0; version <= static_cast<uint32_t>(eCharacterVersion::PET_IDS); version++) EXPECT_TRUE(GetsNewItemIdsAtLogin(version)) << version;
	EXPECT_FALSE(GetsNewItemIdsAtLogin(static_cast<uint32_t>(eCharacterVersion::INVENTORY_PERSISTENT_IDS)));
	EXPECT_FALSE(GetsNewItemIdsAtLogin(static_cast<uint32_t>(eCharacterVersion::UP_TO_DATE)));

	const Location oldSave{ Location::eKind::CHARACTER, 1, 0, 0, 5, 1, 4 };
	const Location migrated{ Location::eKind::CHARACTER, 1, 0, 0, 5, 1, 9 };
	const Location mail{ Location::eKind::MAIL, 1, 77, 0, 5, 1, 0 };
	EXPECT_TRUE(oldSave.NewIdAtLogin());
	EXPECT_FALSE(migrated.NewIdAtLogin());
	EXPECT_FALSE(mail.NewIdAtLogin()); // the migration only touches the character's inventories
}

TEST(EconomyScanTests, ResolvesCollisionsOnLogin) {
	constexpr uint32_t OLD = 5, NEW = 9;
	const auto copy = [](LWOOBJID owner, LOT lot, uint32_t version, Location::eKind kind = Location::eKind::CHARACTER) {
		return Location{ kind, owner, 0, 0, lot, 1, version };
	};

	// Both saves are old: either login is enough
	auto fix = ResolveOnLogin({ copy(100, 5182, OLD), copy(200, 6093, OLD) });
	EXPECT_TRUE(fix.resolves);
	EXPECT_EQ(fix.characters, (std::vector<LWOOBJID>{ 100, 200 }));
	EXPECT_EQ(fix.loginsNeeded, 1u);

	// One old save: its login is needed, the migrated copy keeps the id alone
	fix = ResolveOnLogin({ copy(100, 5182, NEW), copy(200, 6093, OLD) });
	EXPECT_TRUE(fix.resolves);
	EXPECT_EQ(fix.characters, (std::vector<LWOOBJID>{ 200 }));
	EXPECT_EQ(fix.loginsNeeded, 1u);

	// Both migrated: logging in changes nothing
	fix = ResolveOnLogin({ copy(100, 5182, NEW), copy(200, 6093, NEW) });
	EXPECT_FALSE(fix.resolves);
	EXPECT_TRUE(fix.characters.empty());

	// A copy in mail keeps its id; with a migrated copy too, two keep it
	fix = ResolveOnLogin({ copy(100, 5182, NEW), copy(300, 6093, 0, Location::eKind::MAIL), copy(200, 5, OLD) });
	EXPECT_FALSE(fix.resolves);
	EXPECT_EQ(fix.characters, (std::vector<LWOOBJID>{ 200 }));
	// Mail and an old save: that login is enough
	fix = ResolveOnLogin({ copy(300, 6093, 0, Location::eKind::MAIL), copy(200, 5, OLD) });
	EXPECT_TRUE(fix.resolves);
	EXPECT_EQ(fix.loginsNeeded, 1u);

	// One migrated copy and two old saves: both must log in
	fix = ResolveOnLogin({ copy(100, 1, NEW), copy(200, 2, OLD), copy(300, 3, OLD) });
	EXPECT_TRUE(fix.resolves);
	EXPECT_EQ(fix.loginsNeeded, 2u);

	// Three old saves with one copy each: any two logins leave one copy with the id
	fix = ResolveOnLogin({ copy(100, 1, OLD), copy(200, 2, OLD), copy(300, 3, OLD) });
	EXPECT_TRUE(fix.resolves);
	EXPECT_EQ(fix.loginsNeeded, 2u);

	// Two copies on one old save (different inventories) and one on another: not any login works, so all are asked for
	fix = ResolveOnLogin({ copy(100, 1, OLD), copy(100, 2, OLD), copy(200, 3, OLD) });
	EXPECT_TRUE(fix.resolves);
	EXPECT_EQ(fix.characters, (std::vector<LWOOBJID>{ 100, 200 }));
	EXPECT_EQ(fix.loginsNeeded, 2u);

	// Every copy on one old save: its login
	fix = ResolveOnLogin({ copy(100, 1, OLD), copy(100, 2, OLD) });
	EXPECT_TRUE(fix.resolves);
	EXPECT_EQ(fix.loginsNeeded, 1u);
}

TEST(EconomyScanTests, DayRangeDefaultsAndClamps) {
	EXPECT_EQ(DayRange(std::nullopt, std::nullopt, 1000, 30, 366), std::make_pair(971u, 1000u));
	EXPECT_EQ(DayRange(990, 995, 1000, 30, 366), std::make_pair(990u, 995u));
	EXPECT_EQ(DayRange(995, 990, 1000, 30, 366), std::make_pair(990u, 995u)); // swapped
	EXPECT_EQ(DayRange(990, 5000, 1000, 30, 366), std::make_pair(990u, 1000u)); // no future
	EXPECT_EQ(DayRange(5000, std::nullopt, 1000, 30, 366), std::make_pair(1000u, 1000u));
	EXPECT_EQ(DayRange(0, 1000, 1000, 30, 366), std::make_pair(635u, 1000u)); // at most maxDays
	EXPECT_EQ(DayRange(std::nullopt, 10, 1000, 30, 366), std::make_pair(0u, 10u)); // no underflow
}

namespace {
	template<typename T>
	void Put(std::string& out, T value) {
		out.append(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	// A chunk in the .raw layout with no color, light or blend maps
	void PutChunk(std::string& out, uint32_t index, uint32_t size, float x, float z, float scale, float (*height)(uint32_t i, uint32_t j)) {
		Put(out, index); Put(out, size); Put(out, size); Put(out, x); Put(out, z);
		for (int i = 0; i < 4; i++) Put<uint32_t>(out, 0);
		Put(out, scale);
		for (uint32_t i = 0; i < size; i++) for (uint32_t j = 0; j < size; j++) Put(out, height(i, j));
		Put<uint32_t>(out, 0); // color map
		Put<uint32_t>(out, 0); // light map
		Put<uint32_t>(out, 0); // color map 2
		Put<uint8_t>(out, 0);
		Put<uint32_t>(out, 0); // blend map
		Put<uint32_t>(out, 0); // points
		Put<uint32_t>(out, 0); // end counter
	}

	std::string TwoChunkTerrain() {
		std::string raw;
		Put<uint8_t>(raw, 0x20); Put<uint16_t>(raw, 0); Put<uint32_t>(raw, 2); Put<uint32_t>(raw, 2); Put<uint32_t>(raw, 1);
		// Chunks side by side along x, sharing their edge; height encodes the world x and z
		PutChunk(raw, 0, 3, -8.0f, 4.0f, 4.0f, [](uint32_t i, uint32_t j) { return static_cast<float>(i) * 10.0f + j; });
		PutChunk(raw, 1, 3, 0.0f, 4.0f, 4.0f, [](uint32_t i, uint32_t j) { return static_cast<float>(i + 2) * 10.0f + j; });
		return raw;
	}
}

TEST(TerrainMapTests, MergesChunksIntoOneGrid) {
	const auto grid = TerrainMap::Parse(TwoChunkTerrain());
	ASSERT_TRUE(grid.has_value());
	EXPECT_FLOAT_EQ(grid->minX, -8.0f);
	EXPECT_FLOAT_EQ(grid->minZ, 4.0f);
	EXPECT_FLOAT_EQ(grid->step, 4.0f);
	ASSERT_EQ(grid->width, 5);
	ASSERT_EQ(grid->height, 3);
	// heights[width * i + j] is x = offset + i * scale, z = offset + j * scale
	for (uint32_t z = 0; z < grid->height; z++) {
		for (uint32_t x = 0; x < grid->width; x++) {
			EXPECT_FLOAT_EQ(grid->heights[z * grid->width + x], x * 10.0f + z) << x << "," << z;
		}
	}
	EXPECT_FLOAT_EQ(grid->minY, 0.0f);
	EXPECT_FLOAT_EQ(grid->maxY, 42.0f);
}

TEST(TerrainMapTests, ThinsLargeGrids) {
	const auto grid = TerrainMap::Parse(TwoChunkTerrain(), 3);
	ASSERT_TRUE(grid.has_value());
	EXPECT_EQ(grid->width, 3);
	EXPECT_EQ(grid->height, 2);
	EXPECT_FLOAT_EQ(grid->step, 8.0f);
	EXPECT_FLOAT_EQ(grid->heights[1], 20.0f); // x index 2
}

TEST(TerrainMapTests, RejectsTruncatedOrOldFiles) {
	const auto raw = TwoChunkTerrain();
	EXPECT_FALSE(TerrainMap::Parse(raw.substr(0, raw.size() - 3)).has_value());
	EXPECT_FALSE(TerrainMap::Parse(raw.substr(0, 40)).has_value());
	auto old = raw;
	old[0] = 0x1f;
	EXPECT_FALSE(TerrainMap::Parse(old).has_value());
	EXPECT_FALSE(TerrainMap::Parse("").has_value());
}

TEST(TerrainMapTests, QuantizesToSixteenBits) {
	TerrainMap::Grid grid;
	grid.width = 3;
	grid.height = 1;
	grid.minY = 10.0f;
	grid.maxY = 20.0f;
	grid.heights = { 10.0f, 20.0f, std::nanf("") };
	const auto packed = TerrainMap::Quantize(grid);
	ASSERT_EQ(packed.size(), 6);
	const auto value = [&](size_t i) { return static_cast<uint16_t>(static_cast<uint8_t>(packed[i * 2]) | (static_cast<uint8_t>(packed[i * 2 + 1]) << 8)); };
	EXPECT_EQ(value(0), 0);
	EXPECT_EQ(value(1), 65534);
	EXPECT_EQ(value(2), 65535);
}

#include "IEconomyLedger.h"

TEST(EconomyPlaceTests, PlaceFilterSql) {
	// Everywhere: no condition
	EXPECT_EQ(IEconomyLedger::PlaceFilter{}.Sql(), "");
	// A property zone's properties together, one property, clone 0 (rows from before properties were told apart)
	EXPECT_EQ((IEconomyLedger::PlaceFilter{ { 1150 }, std::nullopt }).Sql(), " AND zone IN (1150)");
	EXPECT_EQ((IEconomyLedger::PlaceFilter{ { 1150 }, 7u }).Sql(), " AND zone IN (1150) AND clone_id = 7");
	EXPECT_EQ((IEconomyLedger::PlaceFilter{ { 1150 }, 0u }).Sql(), " AND zone IN (1150) AND clone_id = 0");
	// Every property zone, one owner's clone
	EXPECT_EQ((IEconomyLedger::PlaceFilter{ { 1150, 1151, 1250 }, 12u }).Sql(), " AND zone IN (1150,1151,1250) AND clone_id = 12");
}
