#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>

#include "AssetManager.h"
#include "dCommonDependencies.h"
#include "LevelFile.h"
#include "ZoneEditLock.h"
#include "ZoneOverrideStore.h"
#include "ZonePublish.h"

namespace fs = std::filesystem;

namespace {
	struct Bytes {
		std::string data;
		template<typename T> Bytes& Put(T value) { data.append(reinterpret_cast<const char*>(&value), sizeof(T)); return *this; }
		template<typename T> void At(size_t pos, T value) { std::memcpy(data.data() + pos, &value, sizeof(T)); }
		Bytes& Text(const std::string& text) { Put<uint8_t>(static_cast<uint8_t>(text.size())); data += text; return *this; }
		Bytes& Point(float x, float y, float z) { return Put(x).Put(y).Put(z); }
	};

	const std::string FOLDER = "01_live_maps/test_zone/";
	const std::string LUZ = FOLDER + "test_zone.luz";

	// A version 41 zone file with these scenes and terrain file
	std::string ZoneBytes(const std::vector<std::string>& scenes, const std::string& terrain = "test_zone.raw") {
		Bytes w;
		w.Put<uint32_t>(41).Put<uint32_t>(3).Put<uint32_t>(1900);
		w.Point(1, 2, 3).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f);
		w.Put<uint32_t>(static_cast<uint32_t>(scenes.size()));
		uint32_t id = 0;
		for (const auto& scene : scenes) w.Text(scene).Put<uint32_t>(id++).Put<uint32_t>(0).Text("Scene").Put<uint8_t>(1).Put<uint8_t>(2).Put<uint8_t>(3);
		w.Put<uint8_t>(0).Text(terrain).Text("Name").Text("Description");
		w.Put<uint32_t>(0);                    // scene transitions
		w.Put<uint32_t>(8).Put<uint32_t>(1).Put<uint32_t>(0); // the path chunk: version 1, no paths
		return w.data;
	}

	// A scene file from before chunks (version 30) with these objects' settings
	std::string LevelBytes(const std::vector<std::string>& objects) {
		Bytes w;
		w.Put<uint16_t>(30).Put<uint16_t>(0);
		w.data.append(48 + 12, '\0');
		w.Put<uint32_t>(0).Put<uint32_t>(static_cast<uint32_t>(objects.size()));
		int32_t lot = 1;
		for (const auto& settings : objects) {
			w.Put<int64_t>(lot * 10).Put<int32_t>(lot);
			lot++;
			w.Point(0, 0, 0).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put(1.0f);
			w.Put<uint32_t>(static_cast<uint32_t>(settings.size()));
			for (char c : settings) w.Put<uint16_t>(static_cast<uint16_t>(c));
			w.Put<uint32_t>(0);
		}
		return w.data;
	}

	const char* GOOD_TRIGGERS = "<triggers nextID=\"2\"><trigger id=\"1\" enabled=\"1\">"
		"<event id=\"OnEnter\"><command id=\"fireEvent\" target=\"self\" args=\"go\"/></event></trigger></triggers>";

	// The readers log what they refuse
	struct Fixture : dCommonDependenciesTest {
		fs::path dir;
		fs::path root;
		ZonePublish::Zone zone;
		ZonePublish::Limits limits;
		std::vector<ZonePublish::Upload> uploads;
		uint32_t counter = 0;

		void SetUp() override {
			SetUpDependencies();
			dir = fs::temp_directory_path() / ("dlu-zone-publish-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "-" +
				::testing::UnitTest::GetInstance()->current_test_info()->name());
			fs::remove_all(dir);
			fs::create_directories(dir / "in");
			root = dir / "override";
			zone.zoneId = 1900;
			zone.luzPath = LUZ;
			zone.clientScenes = { FOLDER + "a.lvl", FOLDER + "b.lvl" };
			zone.hasFile = [](const std::string&) { return true; };
		}
		void TearDown() override {
			fs::remove_all(dir);
			TearDownDependencies();
			Game::logger = nullptr;
		}

		void Add(const std::string& name, const std::string& data) {
			const auto file = dir / "in" / std::to_string(counter++);
			std::ofstream(file, std::ios::binary) << data;
			uploads.push_back({ name, file, data.size() });
		}
		void AddGoodSet() {
			Add(LUZ, ZoneBytes({ "a.lvl", "b.lvl" }));
			Add(FOLDER + "a.lvl", LevelBytes({ "a=0:b" }));
			Add(FOLDER + "a.lutriggers", GOOD_TRIGGERS);
		}
		ZonePublish::Result Run() { return ZonePublish::Validate(zone, uploads, limits); }

		static bool Has(const ZonePublish::Result& result, const std::string& path, const std::string& code) {
			for (const auto& file : result.files) {
				if (file.path != path) continue;
				for (const auto& error : file.errors) if (error.code == code) return true;
			}
			return false;
		}

		// Apply what Validate passed, as a transport would
		ZoneOverrideStore::Applied Apply(const ZonePublish::Result& result, const std::string& id, int64_t now, uint32_t keep = 20, const std::string& note = "") {
			std::vector<ZoneOverrideStore::NewFile> files;
			for (const auto& file : result.files) files.push_back({ file.path, file.source, file.bytes, file.sha256, file.version });
			return ZoneOverrideStore::Apply(root, { zone.zoneId, FOLDER, id, "editor", note, now, keep }, files);
		}
		static std::string Slurp(const fs::path& path) {
			std::ifstream in(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(in), {});
		}
		// Every file under the override folder's maps/, for "nothing was written"
		std::vector<std::string> Written() const {
			std::vector<std::string> files;
			std::error_code code;
			for (fs::recursive_directory_iterator it(root, code), end; !code && it != end; it.increment(code)) {
				if (it->is_regular_file()) files.push_back(it->path().lexically_relative(root).generic_string());
			}
			return files;
		}
	};
	using ZonePublishTests = Fixture;
	using ZoneOverrideStoreTests = Fixture;
}

TEST(ZonePublishNameTests, RefusesTraversalAbsoluteBackslashAndNul) {
	EXPECT_FALSE(ZonePublish::NameProblem("01_live_maps/test_zone/a.lvl"));
	EXPECT_TRUE(ZonePublish::NameProblem(""));
	EXPECT_TRUE(ZonePublish::NameProblem("../a.lvl"));
	EXPECT_TRUE(ZonePublish::NameProblem("01_live_maps/../a.lvl"));
	EXPECT_TRUE(ZonePublish::NameProblem("01_live_maps/test_zone/.."));
	EXPECT_TRUE(ZonePublish::NameProblem("./a.lvl"));
	EXPECT_TRUE(ZonePublish::NameProblem("/etc/passwd"));
	EXPECT_TRUE(ZonePublish::NameProblem("C:/windows/a.lvl"));
	EXPECT_TRUE(ZonePublish::NameProblem("01_live_maps\\test_zone\\a.lvl"));
	EXPECT_TRUE(ZonePublish::NameProblem(std::string("a.lvl\0.luz", 10)));
	EXPECT_TRUE(ZonePublish::NameProblem("a//b.lvl"));
	EXPECT_TRUE(ZonePublish::NameProblem("a/b.lvl/"));
	EXPECT_TRUE(ZonePublish::NameProblem("a/b.lvl."));
	EXPECT_TRUE(ZonePublish::NameProblem("a/ b.lvl"));
	EXPECT_TRUE(ZonePublish::NameProblem("a/b\n.lvl"));
	EXPECT_TRUE(ZonePublish::NameProblem("a/b\xC3\xA9.lvl")); // no bytes outside ASCII, so no look-alike letters
	EXPECT_TRUE(ZonePublish::NameProblem(std::string(ZonePublish::MAX_NAME_LENGTH + 1, 'a')));
	EXPECT_EQ(ZonePublish::Canonical("01_Live_Maps/Test_Zone/A.LVL"), "01_live_maps/test_zone/a.lvl");
}

TEST_F(ZonePublishTests, ResolveInsideFollowsLinksOutOfTheRoot) {
	fs::create_directories(root / "maps");
	fs::create_directories(dir / "elsewhere");
	EXPECT_TRUE(ZonePublish::ResolveInside(root / "maps", "zone/a.lvl"));
	EXPECT_FALSE(ZonePublish::ResolveInside(root / "maps", "../a.lvl"));
	EXPECT_FALSE(ZonePublish::ResolveInside(root / "maps", "/a.lvl"));
	// A link inside the root that points out of it is followed before the check
	std::error_code code;
	fs::create_directory_symlink(dir / "elsewhere", root / "maps" / "link", code);
	if (!code) EXPECT_FALSE(ZonePublish::ResolveInside(root / "maps", "link/a.lvl"));
}

TEST_F(ZonePublishTests, GoodSetPasses) {
	AddGoodSet();
	const auto result = Run();
	ASSERT_TRUE(result.ok);
	ASSERT_EQ(result.files.size(), 3u);
	EXPECT_EQ(result.files[0].path, LUZ);
	EXPECT_EQ(result.files[0].version, 41u);
	EXPECT_EQ(result.files[1].version, 30u);
	EXPECT_EQ(result.files[0].sha256.size(), 64u);
	EXPECT_EQ(result.files[0].sha256, ZonePublish::Sha256(ZoneBytes({ "a.lvl", "b.lvl" })));
	EXPECT_EQ(result.objects, 1u);
}

TEST(ZonePublishHashTests, Sha256MatchesKnownValue) {
	EXPECT_EQ(ZonePublish::Sha256("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_F(ZonePublishTests, NamesAreCheckedBeforeAnythingIsRead) {
	Add(LUZ, ZoneBytes({ "a.lvl" }));
	Add("../../etc/passwd", "x");
	Add("/etc/passwd", "x");
	Add("01_live_maps\\test_zone\\a.lvl", LevelBytes({}));
	Add(std::string("01_live_maps/test_zone/a.lvl\0x", 30), LevelBytes({}));
	Add("01_live_maps/other_zone/a.lvl", LevelBytes({}));       // another zone's folder
	Add(FOLDER + "c.lvl", LevelBytes({}));                      // not a scene the .luz lists
	Add(FOLDER + "notes.txt", "x");
	const auto result = Run();
	EXPECT_FALSE(result.ok);
	EXPECT_TRUE(result.files[0].ok);
	for (size_t i = 1; i < result.files.size(); ++i) {
		ASSERT_FALSE(result.files[i].ok) << result.files[i].path;
		EXPECT_EQ(result.files[i].errors[0].code, "name_not_allowed") << result.files[i].path;
	}
}

TEST_F(ZonePublishTests, CaseTricksAreDuplicates) {
	AddGoodSet();
	Add("01_Live_Maps/Test_Zone/A.LVL", LevelBytes({}));
	const auto result = Run();
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(result.files[3].errors[0].code, "duplicate");
}

TEST_F(ZonePublishTests, MissingLuz) {
	Add(FOLDER + "a.lvl", LevelBytes({}));
	const auto result = Run();
	EXPECT_FALSE(result.ok);
	EXPECT_TRUE(Has(result, LUZ, "missing_luz"));
}

TEST_F(ZonePublishTests, UnknownSceneIsASceneMismatch) {
	// The .luz lists a scene the zone doesn't have in the client's files
	Add(LUZ, ZoneBytes({ "a.lvl", "new_scene.lvl" }));
	Add(FOLDER + "new_scene.lvl", LevelBytes({}));
	const auto result = Run();
	EXPECT_FALSE(result.ok);
	EXPECT_TRUE(Has(result, LUZ, "scene_mismatch"));
}

TEST_F(ZonePublishTests, SceneNamedOutsideTheFolderIsASceneMismatch) {
	Add(LUZ, ZoneBytes({ "../other_zone/a.lvl" }));
	const auto result = Run();
	EXPECT_TRUE(Has(result, LUZ, "scene_mismatch"));
}

TEST_F(ZonePublishTests, SceneTheZoneLacksIsASceneMismatch) {
	// Listed, a scene of the zone, but neither in the set nor on the server
	zone.hasFile = [](const std::string& path) { return !path.ends_with("b.lvl"); };
	AddGoodSet();
	const auto result = Run();
	EXPECT_FALSE(result.ok);
	EXPECT_TRUE(Has(result, LUZ, "scene_mismatch"));
}

TEST_F(ZonePublishTests, TerrainMismatch) {
	Add(LUZ, ZoneBytes({ "a.lvl" }, "test_zone.raw"));
	Add(FOLDER + "other.raw", "x");
	auto result = Run();
	EXPECT_TRUE(Has(result, FOLDER + "other.raw", "terrain_mismatch"));

	uploads.clear();
	Add(LUZ, ZoneBytes({ "a.lvl" }, "../elsewhere.raw"));
	result = Run();
	EXPECT_TRUE(Has(result, LUZ, "terrain_mismatch"));

	uploads.clear();
	zone.hasFile = [](const std::string& path) { return !path.ends_with(".raw"); };
	Add(LUZ, ZoneBytes({ "a.lvl" }));
	result = Run();
	EXPECT_TRUE(Has(result, LUZ, "terrain_mismatch"));
}

TEST_F(ZonePublishTests, TruncatedFilesFailToParse) {
	auto luz = ZoneBytes({ "a.lvl" });
	Add(LUZ, luz.substr(0, luz.size() - 3));
	auto result = Run();
	EXPECT_TRUE(Has(result, LUZ, "parse_failed"));

	uploads.clear();
	auto level = LevelBytes({ "a=0:b", "c=0:d" });
	Add(LUZ, luz);
	Add(FOLDER + "a.lvl", level.substr(0, level.size() - 2));
	result = Run();
	EXPECT_FALSE(result.ok);
	EXPECT_TRUE(Has(result, FOLDER + "a.lvl", "parse_failed"));
	EXPECT_TRUE(result.files[1].errors[0].offset.has_value());

	uploads.clear();
	Add(LUZ, luz + "extra");
	result = Run();
	EXPECT_TRUE(Has(result, LUZ, "parse_failed"));

	uploads.clear();
	Add(LUZ, luz);
	Add(FOLDER + "test_zone.raw", "not a terrain file");
	result = Run();
	EXPECT_TRUE(Has(result, FOLDER + "test_zone.raw", "parse_failed"));
}

TEST_F(ZonePublishTests, UnsupportedVersions) {
	auto luz = ZoneBytes({ "a.lvl" });
	const uint32_t version = 99;
	std::memcpy(luz.data(), &version, 4);
	Add(LUZ, luz);
	auto result = Run();
	EXPECT_FALSE(result.ok);
	EXPECT_TRUE(Has(result, LUZ, "unsupported_version") || Has(result, LUZ, "parse_failed"));

	uploads.clear();
	Bytes level;
	level.Put<uint16_t>(60).Put<uint16_t>(0).Put<uint8_t>(0).Put<uint32_t>(1);
	Add(LUZ, ZoneBytes({ "a.lvl" }));
	Add(FOLDER + "a.lvl", level.data);
	result = Run();
	EXPECT_TRUE(Has(result, FOLDER + "a.lvl", "unsupported_version"));
}

TEST_F(ZonePublishTests, TriggerChecks) {
	Add(LUZ, ZoneBytes({ "a.lvl" }));
	Add(FOLDER + "a.lutriggers", "<triggers><trigger id=\"1\" enabled=\"1\">"
		"<event id=\"OnEnter\"><command id=\"fireEvent\" args=\"go\"/></event>"
		"<event id=\"OnMadeUp\"><command id=\"madeUpCommand\" target=\"self\"/></event></trigger></triggers>");
	auto result = Run();
	EXPECT_FALSE(result.ok);
	EXPECT_TRUE(Has(result, FOLDER + "a.lutriggers", "trigger_missing_target"));
	EXPECT_TRUE(Has(result, FOLDER + "a.lutriggers", "unknown_trigger_event"));
	EXPECT_TRUE(Has(result, FOLDER + "a.lutriggers", "unknown_trigger_command"));

	EXPECT_EQ(ZonePublish::CheckTriggers("<triggers><trigger id=\"1\"").at(0).code, "parse_failed");
	EXPECT_EQ(ZonePublish::CheckTriggers("<other/>").at(0).code, "parse_failed");
	EXPECT_TRUE(ZonePublish::CheckTriggers("").empty());
	EXPECT_TRUE(ZonePublish::CheckTriggers(GOOD_TRIGGERS).empty());
}

TEST_F(ZonePublishTests, BadObjectConfigAndObjectCap) {
	Add(LUZ, ZoneBytes({ "a.lvl", "b.lvl" }));
	Add(FOLDER + "a.lvl", LevelBytes({ "good=0:yes", "broken=:" }));
	auto result = Run();
	EXPECT_TRUE(Has(result, FOLDER + "a.lvl", "bad_object_config"));

	uploads.clear();
	limits.maxObjects = 3;
	Add(LUZ, ZoneBytes({ "a.lvl", "b.lvl" }));
	Add(FOLDER + "a.lvl", LevelBytes({ "a=0:b", "a=0:b" }));
	Add(FOLDER + "b.lvl", LevelBytes({ "a=0:b", "a=0:b" }));
	result = Run();
	EXPECT_FALSE(result.ok);
	EXPECT_TRUE(Has(result, FOLDER + "b.lvl", "too_many_objects"));
}

TEST_F(ZonePublishTests, SizeLimits) {
	limits.maxFileBytes = 64;
	Add(LUZ, ZoneBytes({ "a.lvl" }));
	Add(FOLDER + "a.lvl", LevelBytes({ std::string(200, 'x') + "=0:b" }));
	auto result = Run();
	EXPECT_TRUE(Has(result, FOLDER + "a.lvl", "too_large"));

	limits = {};
	limits.maxFiles = 1;
	result = Run();
	EXPECT_TRUE(Has(result, FOLDER + "a.lvl", "too_large"));

	limits = {};
	limits.maxTotalBytes = uploads[0].bytes + 1;
	result = Run();
	EXPECT_TRUE(Has(result, FOLDER + "a.lvl", "too_large"));
}

// One bad file refuses the whole set, and nothing of it is written
TEST_F(ZonePublishTests, AllOrNothing) {
	AddGoodSet();
	Add(FOLDER + "b.lvl", "garbage");
	const auto result = Run();
	EXPECT_FALSE(result.ok);
	EXPECT_TRUE(result.files[0].ok);
	EXPECT_TRUE(result.files[1].ok);
	EXPECT_FALSE(result.files[3].ok);
	EXPECT_TRUE(Written().empty());
	EXPECT_TRUE(ZoneOverrideStore::Current(root, zone.zoneId, FOLDER).empty());
}

TEST_F(ZoneOverrideStoreTests, ApplyPutsTheSetWhereTheAssetManagerReads) {
	AddGoodSet();
	const auto level = Slurp(uploads[1].file);
	const auto applied = Apply(Run(), "p1", 1000);
	EXPECT_FALSE(applied.backupId);
	ASSERT_EQ(applied.files.size(), 3u);
	EXPECT_EQ(Slurp(root / "maps" / FOLDER / "a.lvl"), level);
	EXPECT_FALSE(fs::exists(root / "staging" / "p1"));

	const auto current = ZoneOverrideStore::Current(root, zone.zoneId, FOLDER);
	ASSERT_EQ(current.size(), 3u);
	EXPECT_EQ(current[1].path, FOLDER + "a.lvl");
	EXPECT_EQ(current[1].bytes, level.size());
	EXPECT_EQ(current[1].sha256, ZonePublish::Sha256(level));
	EXPECT_EQ(current[1].who, "editor");
	EXPECT_EQ(current[1].when, 1000);

	// The asset manager serves it for a name under maps/, whatever the name's case, and nothing else
	const auto name = AssetManager::OverrideName("maps\\01_Live_Maps\\Test_Zone\\A.LVL");
	ASSERT_TRUE(name);
	EXPECT_EQ(Slurp(root / *name), level);
	EXPECT_FALSE(AssetManager::OverrideName("macros/a.scm"));
	EXPECT_FALSE(AssetManager::OverrideName("maps/../cdclient.fdb"));
	EXPECT_FALSE(AssetManager::OverrideName("maps//a.lvl"));
}

TEST_F(ZoneOverrideStoreTests, ASecondSetReplacesTheWholeFirstAndBacksItUp) {
	AddGoodSet();
	Apply(Run(), "p1", 1000, 20, "first");
	uploads.clear();
	Add(LUZ, ZoneBytes({ "a.lvl" }));
	Add(FOLDER + "a.lvl", LevelBytes({ "second=0:set" }));
	const auto second = Slurp(uploads[1].file);
	const auto applied = Apply(Run(), "p2", 2000, 20, "second");
	ASSERT_TRUE(applied.backupId);
	EXPECT_EQ(*applied.backupId, "2000-p2");
	EXPECT_EQ(Slurp(root / "maps" / FOLDER / "a.lvl"), second);
	// The first set's triggers are not part of the second: gone from what is served
	EXPECT_FALSE(fs::exists(root / "maps" / FOLDER / "a.lutriggers"));

	const auto backups = ZoneOverrideStore::Backups(root, zone.zoneId);
	ASSERT_EQ(backups.size(), 1u);
	EXPECT_EQ(backups[0].id, "2000-p2");
	EXPECT_EQ(backups[0].time, 2000);
	EXPECT_EQ(backups[0].who, "editor");
	EXPECT_EQ(backups[0].note, "first");
	EXPECT_EQ(backups[0].files.size(), 3u);
}

TEST_F(ZoneOverrideStoreTests, RestoreAndClearAreOneCallEach) {
	AddGoodSet();
	const auto first = Slurp(uploads[1].file);
	Apply(Run(), "p1", 1000);
	uploads.clear();
	Add(LUZ, ZoneBytes({ "a.lvl" }));
	Add(FOLDER + "a.lvl", LevelBytes({ "second=0:set" }));
	const auto backupId = *Apply(Run(), "p2", 2000).backupId;

	const auto restored = ZoneOverrideStore::Restore(root, { zone.zoneId, FOLDER, "r1", "admin", "", 3000, 20 }, backupId);
	EXPECT_EQ(restored.files.size(), 3u);
	EXPECT_EQ(Slurp(root / "maps" / FOLDER / "a.lvl"), first);
	EXPECT_TRUE(fs::exists(root / "maps" / FOLDER / "a.lutriggers"));
	// What the restore replaced is itself kept
	ASSERT_TRUE(restored.backupId);
	EXPECT_EQ(ZoneOverrideStore::Backups(root, zone.zoneId).size(), 2u);

	EXPECT_THROW(ZoneOverrideStore::Restore(root, { zone.zoneId, FOLDER, "r2", "admin", "", 3500, 20 }, "9999-nothere"), ZoneOverrideStore::StoreError);
	EXPECT_THROW(ZoneOverrideStore::Restore(root, { zone.zoneId, FOLDER, "r3", "admin", "", 3500, 20 }, "../../maps"), ZoneOverrideStore::StoreError);
	EXPECT_EQ(Slurp(root / "maps" / FOLDER / "a.lvl"), first);

	const auto cleared = ZoneOverrideStore::Clear(root, { zone.zoneId, FOLDER, "c1", "admin", "", 4000, 20 });
	EXPECT_TRUE(cleared.files.empty());
	EXPECT_TRUE(cleared.backupId);
	EXPECT_FALSE(fs::exists(root / "maps" / FOLDER / "a.lvl"));
	EXPECT_TRUE(ZoneOverrideStore::Current(root, zone.zoneId, FOLDER).empty());
	EXPECT_EQ(ZoneOverrideStore::Backups(root, zone.zoneId).size(), 3u);
}

TEST_F(ZoneOverrideStoreTests, RestoreRefusesABackupThatWasChanged) {
	AddGoodSet();
	Apply(Run(), "p1", 1000);
	const auto backupId = *ZoneOverrideStore::Clear(root, { zone.zoneId, FOLDER, "c1", "admin", "", 2000, 20 }).backupId;
	const auto kept = root / "backups" / "1900" / backupId / "files" / FOLDER / "a.lvl";
	fs::remove(kept);
	std::ofstream(kept, std::ios::binary) << "changed";
	EXPECT_THROW(ZoneOverrideStore::Restore(root, { zone.zoneId, FOLDER, "r1", "admin", "", 3000, 20 }, backupId), ZoneOverrideStore::StoreError);
	EXPECT_TRUE(ZoneOverrideStore::Current(root, zone.zoneId, FOLDER).empty());
}

TEST_F(ZoneOverrideStoreTests, KeepsTheNewestBackups) {
	for (int i = 0; i < 6; ++i) {
		uploads.clear();
		Add(LUZ, ZoneBytes({ "a.lvl" }));
		Add(FOLDER + "a.lvl", LevelBytes({ "set=1:" + std::to_string(i) }));
		Apply(Run(), "p" + std::to_string(i), 1000 + i, 3);
	}
	const auto backups = ZoneOverrideStore::Backups(root, zone.zoneId);
	ASSERT_EQ(backups.size(), 3u);
	EXPECT_EQ(backups[0].id, "1005-p5");
	EXPECT_EQ(backups[2].id, "1003-p3");
}

TEST_F(ZoneOverrideStoreTests, AnotherZoneInTheSameFolderKeepsItsSet) {
	AddGoodSet();
	Apply(Run(), "p1", 1000);
	// Zone 1901 shares the folder
	const auto other = FOLDER + "other_zone.luz";
	const auto file = dir / "in" / "other";
	std::ofstream(file, std::ios::binary) << "other zone";
	ZoneOverrideStore::Apply(root, { 1901, FOLDER, "o1", "editor", "", 1500, 20 }, { { other, file, 10, ZonePublish::Sha256("other zone"), 41 } });
	EXPECT_EQ(ZoneOverrideStore::Current(root, 1900, FOLDER).size(), 3u);
	ZoneOverrideStore::Clear(root, { 1900, FOLDER, "c1", "admin", "", 2000, 20 });
	EXPECT_EQ(Slurp(root / "maps" / other), "other zone");
	EXPECT_EQ(ZoneOverrideStore::Current(root, 1901, FOLDER).size(), 1u);
	EXPECT_FALSE(fs::exists(root / "maps" / FOLDER / "a.lvl"));
}

TEST_F(ZoneOverrideStoreTests, RefusesNamesOutsideTheZoneFolderAndBadIds) {
	const auto file = dir / "in" / "x";
	std::ofstream(file) << "x";
	const ZoneOverrideStore::Change change{ zone.zoneId, FOLDER, "p1", "editor", "", 1000, 20 };
	EXPECT_THROW(ZoneOverrideStore::Apply(root, change, { { "01_live_maps/other_zone/a.lvl", file, 1, "", 0 } }), ZoneOverrideStore::StoreError);
	EXPECT_THROW(ZoneOverrideStore::Apply(root, change, { { FOLDER + "../../../escape", file, 1, "", 0 } }), ZoneOverrideStore::StoreError);
	EXPECT_THROW(ZoneOverrideStore::Apply(root, change, { { FOLDER + "A.lvl", file, 1, "", 0 } }), ZoneOverrideStore::StoreError);
	auto bad = change;
	bad.id = "../x";
	EXPECT_THROW(ZoneOverrideStore::Apply(root, bad, { { FOLDER + "a.lvl", file, 1, "", 0 } }), ZoneOverrideStore::StoreError);
	bad = change;
	bad.zoneFolder = "../";
	EXPECT_THROW(ZoneOverrideStore::Apply(root, bad, { { "../a.lvl", file, 1, "", 0 } }), ZoneOverrideStore::StoreError);
	EXPECT_FALSE(fs::exists(dir / "escape"));
	EXPECT_FALSE(ZoneOverrideStore::ValidId(""));
	EXPECT_FALSE(ZoneOverrideStore::ValidId("a/b"));
	EXPECT_FALSE(ZoneOverrideStore::ValidId("A"));
	EXPECT_TRUE(ZoneOverrideStore::ValidId("1000-ab12"));
}

TEST(ZoneEditLockTests, TakeReleaseAndChange) {
	using namespace ZoneEditLock;
	const Levels levels{ 6, 9 };
	const Actor editor{ 10, 6, true }, other{ 11, 6, true }, stranger{ 12, 6, false }, low{ 13, 5, true }, admin{ 14, 9, false };

	// Taking: an assigned editor at the level, or an admin
	EXPECT_EQ(Take(editor, std::nullopt, levels), eResult::OK);
	EXPECT_EQ(Take(admin, std::nullopt, levels), eResult::OK);
	EXPECT_EQ(Take(stranger, std::nullopt, levels), eResult::NOT_ASSIGNED);
	EXPECT_EQ(Take(low, std::nullopt, levels), eResult::LEVEL_TOO_LOW);
	EXPECT_EQ(Take(other, 10u, levels), eResult::LOCKED_BY_OTHER);
	EXPECT_EQ(Take(admin, 10u, levels), eResult::LOCKED_BY_OTHER); // an admin releases it first
	EXPECT_EQ(Take(editor, 10u, levels), eResult::NO_CHANGE);
	EXPECT_EQ(Take(Actor{ 0, 9, true }, std::nullopt, levels), eResult::LEVEL_TOO_LOW);

	// Releasing: the holder, or an admin forcing it
	EXPECT_EQ(Release(editor, 10u, levels), eResult::OK);
	EXPECT_EQ(Release(other, 10u, levels), eResult::LOCKED_BY_OTHER);
	EXPECT_EQ(Release(admin, 10u, levels), eResult::OK);
	EXPECT_EQ(Release(editor, std::nullopt, levels), eResult::NO_CHANGE);
	EXPECT_EQ(Release(low, 13u, levels), eResult::LEVEL_TOO_LOW);

	// Changing the files: the holder only, admin or not
	EXPECT_EQ(Change(editor, 10u, levels), eResult::OK);
	EXPECT_EQ(Change(other, 10u, levels), eResult::LOCKED_BY_OTHER);
	EXPECT_EQ(Change(admin, 10u, levels), eResult::LOCKED_BY_OTHER);
	EXPECT_EQ(Change(editor, std::nullopt, levels), eResult::NOT_ASSIGNED);
	EXPECT_EQ(Change(low, 13u, levels), eResult::LEVEL_TOO_LOW);

	// The numbers are told to requesters
	EXPECT_EQ(static_cast<uint8_t>(eResult::OK), 0);
	EXPECT_EQ(static_cast<uint8_t>(eResult::LEVEL_TOO_LOW), 1);
	EXPECT_EQ(static_cast<uint8_t>(eResult::NOT_ASSIGNED), 2);
	EXPECT_EQ(static_cast<uint8_t>(eResult::LOCKED_BY_OTHER), 3);
	EXPECT_EQ(static_cast<uint8_t>(eResult::NO_CHANGE), 4);
}

TEST(ZoneEditLockTests, ReloadHoldsWhilePlayersAreThere) {
	using namespace ZoneEditLock;
	EXPECT_EQ(PlanReload(false, 5, true), eReload::SKIPPED);
	EXPECT_EQ(PlanReload(true, 0, false), eReload::STARTED);
	EXPECT_EQ(PlanReload(true, 3, false), eReload::HELD);
	EXPECT_EQ(PlanReload(true, 3, true), eReload::STARTED);
}

// ---- Files sent by the game client's level editor (ZoneFilePush): the same checks, store and backups ----

#include "ZoneFilePush.h"

namespace {
	struct PushFixture : Fixture {
		uint32_t pushes = 0;

		ZoneFilePush::Job Job(ZoneFilePack::eFileType type, const ZoneFilePack::Files& files) {
			ZoneFilePush::Job job;
			job.zoneId = zone.zoneId;
			job.luzPath = LUZ;
			job.fileType = type;
			job.packedFiles = ZoneFilePack::Pack(type, files);
			job.root = root;
			job.id = "w" + std::to_string(++pushes);
			job.who = "editor";
			job.now = 1000 + pushes;
			job.clientScenes = zone.clientScenes;
			job.clientFiles = { FOLDER + "a.lvl", FOLDER + "b.lvl", FOLDER + "test_zone.raw" };
			job.clientLuz = ZoneBytes({ "a.lvl", "b.lvl" });
			return job;
		}
	};
	using ZoneFilePushTests = PushFixture;
}

// One scene sent: the set is that scene and the zone's .luz (the client's own, as none was sent)
TEST_F(ZoneFilePushTests, OneFileBecomesASetWithTheZoneFile) {
	ZoneFilePack::Files files;
	files.scenes = { { "A.lvl", LevelBytes({ "pushed=0:yes" }) } };
	const auto result = ZoneFilePush::Run(Job(ZoneFilePack::eFileType::SCENE, files));
	ASSERT_TRUE(result.applied) << result.problem;
	EXPECT_EQ(result.files, 2u);
	EXPECT_FALSE(result.backupId);
	EXPECT_EQ(Slurp(root / "maps" / FOLDER / "a.lvl"), LevelBytes({ "pushed=0:yes" }));
	EXPECT_EQ(Slurp(root / "maps" / LUZ), ZoneBytes({ "a.lvl", "b.lvl" }));
	EXPECT_FALSE(fs::exists(root / "staging" / "w1"));
}

// A second single file keeps the rest of the set, and the set before is a backup
TEST_F(ZoneFilePushTests, OneFileKeepsTheRestOfTheSet) {
	ZoneFilePack::Files first;
	first.scenes = { { "a.lvl", LevelBytes({ "first=0:a" }) } };
	ASSERT_TRUE(ZoneFilePush::Run(Job(ZoneFilePack::eFileType::SCENE, first)).applied);
	ZoneFilePack::Files second;
	second.triggers = { { "a.lutriggers", GOOD_TRIGGERS } };
	const auto result = ZoneFilePush::Run(Job(ZoneFilePack::eFileType::TRIGGERS, second));
	ASSERT_TRUE(result.applied) << result.problem;
	EXPECT_EQ(result.files, 3u);
	ASSERT_TRUE(result.backupId);
	EXPECT_EQ(Slurp(root / "maps" / FOLDER / "a.lvl"), LevelBytes({ "first=0:a" }));
	EXPECT_EQ(Slurp(root / "maps" / FOLDER / "a.lutriggers"), GOOD_TRIGGERS);
	// The backup still holds the first set, whole
	const auto backups = ZoneOverrideStore::Backups(root, zone.zoneId);
	ASSERT_EQ(backups.size(), 1u);
	EXPECT_EQ(backups[0].files.size(), 2u);
	EXPECT_EQ(Slurp(root / "backups" / "1900" / backups[0].id / "files" / FOLDER / "a.lvl"), LevelBytes({ "first=0:a" }));
}

// A whole map is checked whole: one bad file in it and the set the zone has stays as it is
TEST_F(ZoneFilePushTests, ARefusedWholeMapLeavesTheSetAlone) {
	ZoneFilePack::Files first;
	first.triggers = { { "a.lutriggers", GOOD_TRIGGERS } };
	ASSERT_TRUE(ZoneFilePush::Run(Job(ZoneFilePack::eFileType::TRIGGERS, first)).applied);

	ZoneFilePack::Files whole;
	whole.zone = ZoneFilePack::Entry{ "test_zone.luz", ZoneBytes({ "a.lvl" }) };
	whole.terrain = ZoneFilePack::Entry{ "test_zone.raw", "not a terrain file" };
	whole.scenes = { { "a.lvl", LevelBytes({ "whole=0:map" }) } };
	auto result = ZoneFilePush::Run(Job(ZoneFilePack::eFileType::WHOLE_MAP, whole));
	EXPECT_FALSE(result.applied);
	EXPECT_NE(result.problem.find("test_zone.raw"), std::string::npos);
	EXPECT_NE(result.problem.find("parse_failed"), std::string::npos);
	// Refused whole: the set before is untouched
	EXPECT_EQ(Slurp(root / "maps" / FOLDER / "a.lutriggers"), GOOD_TRIGGERS);
	EXPECT_FALSE(fs::exists(root / "maps" / FOLDER / "a.lvl"));
	EXPECT_FALSE(fs::exists(root / "staging" / "w2"));
}

// Whatever folder a sent name has, the file can only become one of this zone's own
TEST_F(ZoneFilePushTests, NamesNeverLeaveTheZoneFolder) {
	ZoneFilePack::Files files;
	files.scenes = { { "x", LevelBytes({}) } };
	// (Pack writes the name as given; a client could send any)
	files.scenes[0].name = "../../../../etc/a.lvl";
	auto result = ZoneFilePush::Run(Job(ZoneFilePack::eFileType::SCENE, files));
	ASSERT_TRUE(result.applied) << result.problem;
	EXPECT_TRUE(fs::exists(root / "maps" / FOLDER / "a.lvl")); // its base name, in the zone's folder
	EXPECT_FALSE(fs::exists(dir / "etc"));

	// A base name the zone has no file for is refused, and so is a file of another kind
	files.scenes[0].name = "..\\other_zone\\c.lvl";
	result = ZoneFilePush::Run(Job(ZoneFilePack::eFileType::SCENE, files));
	EXPECT_FALSE(result.applied);
	EXPECT_NE(result.problem.find("name_not_allowed"), std::string::npos);
	files.scenes[0].name = "passwd";
	result = ZoneFilePush::Run(Job(ZoneFilePack::eFileType::SCENE, files));
	EXPECT_FALSE(result.applied);
	files.scenes[0].name = "..";
	result = ZoneFilePush::Run(Job(ZoneFilePack::eFileType::SCENE, files));
	EXPECT_FALSE(result.applied);
	EXPECT_NE(result.problem.find("unpacked"), std::string::npos);
}

TEST_F(ZoneFilePushTests, DamagedPacksAndFilesAreRefusedWithNothingWritten) {
	ZoneFilePack::Files files;
	files.scenes = { { "a.lvl", LevelBytes({ "a=0:b", "c=0:d" }) } };
	auto job = Job(ZoneFilePack::eFileType::SCENE, files);
	job.packedFiles.resize(job.packedFiles.size() - 3);
	auto result = ZoneFilePush::Run(job);
	EXPECT_FALSE(result.applied);
	EXPECT_NE(result.problem.find("unpacked"), std::string::npos);

	// A scene cut short inside good packing
	auto level = LevelBytes({ "a=0:b", "c=0:d" });
	files.scenes[0].data = level.substr(0, level.size() - 2);
	result = ZoneFilePush::Run(Job(ZoneFilePack::eFileType::SCENE, files));
	EXPECT_FALSE(result.applied);
	EXPECT_NE(result.problem.find("parse_failed"), std::string::npos);

	// Larger than a file may be
	files.scenes[0].data = level;
	job = Job(ZoneFilePack::eFileType::SCENE, files);
	job.limits.maxFileBytes = 16;
	result = ZoneFilePush::Run(job);
	EXPECT_FALSE(result.applied);

	EXPECT_TRUE(ZoneOverrideStore::Current(root, zone.zoneId, FOLDER).empty());
	EXPECT_FALSE(fs::exists(root / "maps" / FOLDER / "a.lvl"));
}
