#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "VanityEvents.h"

using namespace VanityEvents;

namespace {
	VanityXml::Object Npc(const std::string& name, int32_t lot = 2279) {
		VanityXml::Object object;
		object.name = name;
		object.lot = lot;
		object.locations.push_back({ 1200, 1, 2, 3 });
		return object;
	}

	struct TestEvent {
		uint64_t id;
		std::string name;
		int32_t priority;
		std::string file;
		std::string removals;
		std::string fileSwitches;
	};

	// A throwaway vanity folder, removed at the end of the test
	struct Folder {
		std::filesystem::path path;
		explicit Folder(const std::string& test) {
			path = std::filesystem::temp_directory_path() / ("vanity-" + test + "-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
			std::filesystem::remove_all(path);
			std::filesystem::create_directories(path);
		}
		~Folder() { std::filesystem::remove_all(path); }
		void Write(const std::string& name, const std::string& text) const { std::ofstream(path / name) << text; }
	};

	std::string NpcXml(const std::string& name, int lot) {
		return "<object name=\"" + name + "\" lot=\"" + std::to_string(lot) + "\"><locations><location zone=\"1200\" x=\"1\" y=\"2\" z=\"3\" rw=\"1\" rx=\"0\" ry=\"0\" rz=\"0\"/></locations></object>";
	}

	std::vector<std::string> Names(const std::vector<VanityXml::Object>& objects) {
		std::vector<std::string> names;
		for (const auto& object : objects) names.push_back(object.name);
		return names;
	}

	const FileLoad* Find(const World& world, const std::string& name) {
		for (const auto& file : world.files) if (file.name == name) return &file;
		return nullptr;
	}
}

TEST(VanityEventsTests, MergeReplacesRemovesAndAdds) {
	std::vector<VanityXml::Object> base{ Npc("Bob"), Npc("Alice"), Npc(""), Npc("Bob", 4000) };
	Overlay halloween{ "Halloween", { Npc("Bob", 9999), Npc("Pumpkin", 5000), Npc("") }, { "Alice", "Nobody" } };
	const auto merged = Merge(base, { halloween });
	// Both Bobs are replaced by the event's Bob; Alice is taken out; the base's unnamed prop stays
	ASSERT_EQ(merged.objects.size(), 4u);
	EXPECT_EQ(merged.objects[0].name, "");
	EXPECT_EQ(merged.objects[1].name, "Bob");
	EXPECT_EQ(merged.objects[1].lot, 9999);
	EXPECT_EQ(merged.objects[2].name, "Pumpkin");
	EXPECT_EQ(merged.objects[3].name, "");
	EXPECT_TRUE(merged.conflicts.empty());
	ASSERT_EQ(merged.warnings.size(), 1u);
	EXPECT_NE(merged.warnings[0].find("Nobody"), std::string::npos);
}

TEST(VanityEventsTests, MergeOrderAndConflicts) {
	std::vector<TestEvent> events{ { 1, "Moon", 5, "", "" }, { 2, "Halloween", 0, "", "" }, { 3, "Also moon", 5, "", "" } };
	SortForMerge(events);
	ASSERT_EQ(events.size(), 3u);
	EXPECT_EQ(events[0].name, "Halloween"); // lowest priority first
	EXPECT_EQ(events[1].name, "Moon");      // then by id
	EXPECT_EQ(events[2].name, "Also moon");

	std::vector<VanityXml::Object> base{ Npc("Bob") };
	const auto merged = Merge(base, { { "Halloween", { Npc("Bob", 100) }, {} }, { "Moon", { Npc("Bob", 200) }, {} }, { "Also moon", {}, { "Bob" } } });
	EXPECT_TRUE(merged.objects.empty()); // the last one took Bob out
	ASSERT_EQ(merged.conflicts.size(), 1u);
	EXPECT_EQ(merged.conflicts[0].npc, "Bob");
	EXPECT_EQ(merged.conflicts[0].events, (std::vector<std::string>{ "Halloween", "Moon", "Also moon" }));

	const auto two = Merge(base, { { "Halloween", { Npc("Bob", 100) }, {} }, { "Moon", { Npc("Bob", 200) }, {} } });
	ASSERT_EQ(two.objects.size(), 1u);
	EXPECT_EQ(two.objects[0].lot, 200);
}

TEST(VanityEventsTests, SplitNamesAndFileNames) {
	EXPECT_EQ(SplitNames(" Bob \r\n\nAlice\nBob\n"), (std::vector<std::string>{ "Bob", "Alice" }));
	EXPECT_TRUE(ValidFileName("halloween-2026_npcs.xml"));
	EXPECT_FALSE(ValidFileName("../root.xml"));
	EXPECT_FALSE(ValidFileName("a b.xml"));
	EXPECT_FALSE(ValidFileName(".xml"));
	EXPECT_FALSE(ValidFileName("halloween.txt"));
}

TEST(VanityEventsTests, LoadFilesAndOverlays) {
	const Folder folder("load");
	folder.Write("root.xml", R"(<files><file name="a.xml" enabled="1"/><file name="off.xml" enabled="0"/><file name="root.xml" enabled="1"/><file name="../escape.xml" enabled="1"/></files>)");
	folder.Write("a.xml", "<objects>" + NpcXml("Bob", 1) + NpcXml("Alice", 2) + "</objects>");
	folder.Write("off.xml", "<objects>" + NpcXml("Hidden", 3) + "</objects>");
	folder.Write("halloween.xml", R"(<files><file name="extra.xml" enabled="1"/></files><objects>)" + NpcXml("Bob", 10) + "</objects>");
	folder.Write("extra.xml", "<objects>" + NpcXml("Pumpkin", 11) + "</objects>");

	auto base = LoadFiles(folder.path, "root.xml");
	EXPECT_EQ(base.files, (std::vector<std::string>{ "root.xml", "a.xml" }));
	ASSERT_EQ(base.objects.size(), 2u);
	ASSERT_EQ(base.warnings.size(), 1u);
	EXPECT_NE(base.warnings[0].find("../escape.xml"), std::string::npos);

	std::vector<TestEvent> events{ { 1, "Halloween", 0, "halloween.xml", "Alice\n" } };
	const auto world = LoadWorld(folder.path, "root.xml", events);
	EXPECT_EQ(world.fileNpcs, 2u);
	EXPECT_EQ(Names(world.objects), (std::vector<std::string>{ "Pumpkin", "Bob" })); // the overlay's include first, as the worlds load them
	EXPECT_EQ(world.objects[1].lot, 10);
	ASSERT_NE(Find(world, "off.xml"), nullptr);
	EXPECT_FALSE(Find(world, "off.xml")->loaded);
	EXPECT_EQ(Find(world, "off.xml")->includedBy, "root.xml");

	std::vector<TestEvent> missing{ { 2, "Ghost", 0, "nope.xml", "" } };
	const auto withMissing = LoadWorld(folder.path, "root.xml", missing);
	EXPECT_EQ(withMissing.objects.size(), 2u);
	EXPECT_TRUE(std::any_of(withMissing.warnings.begin(), withMissing.warnings.end(), [](const auto& w) { return w.find("nope.xml not found") != std::string::npos; }));
}

TEST(VanityEventsTests, FileSwitchesParse) {
	std::string error;
	const auto switches = ParseFileSwitches(R"({"halloween.xml": true, "summer.xml": false})", error);
	ASSERT_TRUE(switches) << error;
	EXPECT_EQ(*switches, (FileSwitches{ { "halloween.xml", true }, { "summer.xml", false } }));
	EXPECT_EQ(ToJson(*switches).dump(), R"({"halloween.xml":true,"summer.xml":false})");
	EXPECT_TRUE(ParseFileSwitches("", error)->empty());
	EXPECT_FALSE(ParseFileSwitches(R"({"../root.xml": true})", error));
	EXPECT_FALSE(ParseFileSwitches(R"({"a.xml": 1})", error));
	EXPECT_FALSE(ParseFileSwitches(R"(["a.xml"])", error));
	EXPECT_FALSE(ParseFileSwitches("{", error));
}

TEST(VanityEventsTests, FileSwitchOnLoadsAFileNothingNames) {
	const Folder folder("switch-on");
	folder.Write("root.xml", R"(<files><file name="a.xml" enabled="1"/></files>)");
	folder.Write("a.xml", "<objects>" + NpcXml("Bob", 1) + "</objects>");
	folder.Write("zeta.xml", "<objects>" + NpcXml("Zed", 3) + "</objects>");
	folder.Write("halloween.xml", R"(<files><file name="pumpkins.xml" enabled="1"/></files><objects>)" + NpcXml("Witch", 2) + "</objects>");
	folder.Write("pumpkins.xml", "<objects>" + NpcXml("Pumpkin", 4) + "</objects>");

	std::vector<TestEvent> events{ { 1, "Halloween", 0, "", "", R"({"zeta.xml": true, "halloween.xml": true})" } };
	const auto world = LoadWorld(folder.path, "root.xml", events);
	// After the tree, in name order, each with what it includes
	EXPECT_EQ(Names(world.objects), (std::vector<std::string>{ "Bob", "Pumpkin", "Witch", "Zed" }));
	EXPECT_EQ(world.fileNpcs, 4u);
	const auto* halloween = Find(world, "halloween.xml");
	ASSERT_NE(halloween, nullptr);
	EXPECT_TRUE(halloween->loaded);
	EXPECT_EQ(halloween->includedBy, "");
	EXPECT_EQ(halloween->switchedBy, "Halloween");
	EXPECT_EQ(Find(world, "pumpkins.xml")->includedBy, "halloween.xml");
	EXPECT_TRUE(world.warnings.empty());
	EXPECT_TRUE(world.fileConflicts.empty());

	// Without the event nothing changes
	EXPECT_EQ(Names(LoadWorld(folder.path, "root.xml", std::vector<TestEvent>{}).objects), (std::vector<std::string>{ "Bob" }));
}

TEST(VanityEventsTests, FileSwitchOffDropsTheFileAndItsIncludes) {
	const Folder folder("switch-off");
	folder.Write("root.xml", R"(<files><file name="summer.xml" enabled="1"/><file name="shared.xml" enabled="1"/></files>)");
	folder.Write("summer.xml", R"(<files><file name="beach.xml" enabled="1"/><file name="shared.xml" enabled="1"/></files><objects>)" + NpcXml("Surfer", 1) + "</objects>");
	folder.Write("beach.xml", "<objects>" + NpcXml("Lifeguard", 2) + "</objects>");
	folder.Write("shared.xml", "<objects>" + NpcXml("Mayor", 3) + "</objects>");

	EXPECT_EQ(Names(LoadWorld(folder.path, "root.xml", std::vector<TestEvent>{}).objects), (std::vector<std::string>{ "Lifeguard", "Mayor", "Surfer" }));

	std::vector<TestEvent> events{ { 1, "Halloween", 0, "", "", R"({"summer.xml": false, "unknown.xml": false})" } };
	const auto world = LoadWorld(folder.path, "root.xml", events);
	// beach.xml goes with it; shared.xml stays, root.xml includes it too
	EXPECT_EQ(Names(world.objects), (std::vector<std::string>{ "Mayor" }));
	const auto* summer = Find(world, "summer.xml");
	ASSERT_NE(summer, nullptr);
	EXPECT_FALSE(summer->loaded);
	EXPECT_TRUE(summer->enabled);
	EXPECT_EQ(summer->switchedBy, "Halloween");
	EXPECT_EQ(Find(world, "beach.xml"), nullptr);
	// Switching off a file nothing loads is worth a word
	ASSERT_EQ(world.warnings.size(), 1u);
	EXPECT_NE(world.warnings[0].find("unknown.xml"), std::string::npos);
}

TEST(VanityEventsTests, FileSwitchOnANestedInclude) {
	const Folder folder("nested");
	folder.Write("root.xml", R"(<files><file name="town.xml" enabled="1"/></files>)");
	folder.Write("town.xml", R"(<files><file name="lanterns.xml" enabled="0"/><file name="stalls.xml" enabled="1"/></files><objects>)" + NpcXml("Mayor", 1) + "</objects>");
	folder.Write("lanterns.xml", "<objects>" + NpcXml("Lantern", 2) + "</objects>");
	folder.Write("stalls.xml", "<objects>" + NpcXml("Stall", 3) + "</objects>");

	std::vector<TestEvent> events{ { 1, "Festival", 0, "", "", R"({"lanterns.xml": true, "stalls.xml": false})" } };
	const auto world = LoadWorld(folder.path, "root.xml", events);
	// Switched on where town.xml names it, so in its place
	EXPECT_EQ(Names(world.objects), (std::vector<std::string>{ "Lantern", "Mayor" }));
	const auto* lanterns = Find(world, "lanterns.xml");
	ASSERT_NE(lanterns, nullptr);
	EXPECT_TRUE(lanterns->loaded);
	EXPECT_FALSE(lanterns->enabled);
	EXPECT_EQ(lanterns->includedBy, "town.xml");
	EXPECT_EQ(lanterns->switchedBy, "Festival");
	EXPECT_FALSE(Find(world, "stalls.xml")->loaded);
	EXPECT_TRUE(world.warnings.empty());
}

TEST(VanityEventsTests, FileSwitchConflictsGoByPriority) {
	const Folder folder("conflict");
	folder.Write("root.xml", R"(<files><file name="summer.xml" enabled="1"/></files>)");
	folder.Write("summer.xml", "<objects>" + NpcXml("Surfer", 1) + "</objects>");

	// Heat wave has the higher priority, so it is laid on last and keeps summer.xml on
	std::vector<TestEvent> events{ { 2, "Heat wave", 5, "", "", R"({"summer.xml": true})" }, { 1, "Halloween", 0, "", "", R"({"summer.xml": false})" } };
	const auto world = LoadWorld(folder.path, "root.xml", events);
	EXPECT_EQ(world.events, (std::vector<std::string>{ "Halloween", "Heat wave" }));
	EXPECT_EQ(Names(world.objects), (std::vector<std::string>{ "Surfer" }));
	ASSERT_EQ(world.fileConflicts.size(), 1u);
	EXPECT_EQ(world.fileConflicts[0].file, "summer.xml");
	EXPECT_EQ(world.fileConflicts[0].switches, (std::vector<std::pair<std::string, bool>>{ { "Halloween", false }, { "Heat wave", true } }));
	EXPECT_EQ(Find(world, "summer.xml")->switchedBy, "Heat wave");

	events[0].priority = -1;
	const auto flipped = LoadWorld(folder.path, "root.xml", events);
	EXPECT_TRUE(flipped.objects.empty());
	EXPECT_EQ(flipped.fileConflicts[0].switches.back().first, "Halloween");
}

TEST(VanityEventsTests, FileSwitchesWithOverlaysAndRemovals) {
	const Folder folder("combined");
	folder.Write("root.xml", R"(<files><file name="summer.xml" enabled="1"/><file name="town.xml" enabled="1"/></files>)");
	folder.Write("summer.xml", "<objects>" + NpcXml("Surfer", 1) + "</objects>");
	folder.Write("town.xml", "<objects>" + NpcXml("Mayor", 2) + NpcXml("Baker", 3) + "</objects>");
	folder.Write("halloween.xml", "<objects>" + NpcXml("Witch", 4) + "</objects>");
	folder.Write("halloween-look.xml", R"(<files><file name="summer.xml" enabled="1"/></files><objects>)" + NpcXml("Mayor", 40) + "</objects>");

	// The overlay's own include is switched off too; the overlay replaces the Mayor; the Baker is taken out
	std::vector<TestEvent> events{ { 1, "Halloween", 0, "halloween-look.xml", "Baker\n", R"({"halloween.xml": true, "summer.xml": false})" } };
	const auto world = LoadWorld(folder.path, "root.xml", events);
	EXPECT_EQ(world.fileNpcs, 3u); // Mayor, Baker, Witch
	EXPECT_EQ(Names(world.objects), (std::vector<std::string>{ "Witch", "Mayor" }));
	EXPECT_EQ(world.objects[1].lot, 40);
	EXPECT_TRUE(world.warnings.empty());

	// Taking out an NPC that only a switched-on file has works the same
	std::vector<TestEvent> more{ events[0], { 2, "No witches", 1, "", "Witch\n" } };
	const auto both = LoadWorld(folder.path, "root.xml", more);
	EXPECT_EQ(Names(both.objects), (std::vector<std::string>{ "Mayor" }));

	// A switch that can't be read is a warning, and the rest of the event still applies
	std::vector<TestEvent> broken{ { 1, "Broken", 0, "", "Baker\n", "{nope" } };
	const auto withBroken = LoadWorld(folder.path, "root.xml", broken);
	EXPECT_EQ(Names(withBroken.objects), (std::vector<std::string>{ "Surfer", "Mayor" }));
	ASSERT_FALSE(withBroken.warnings.empty());
	EXPECT_NE(withBroken.warnings[0].find("file switches"), std::string::npos);
}
