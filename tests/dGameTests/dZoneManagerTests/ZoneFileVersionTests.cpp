#include "GameDependencies.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "AssetManager.h"
#include "CDZoneTableTable.h"
#include "Zone.h"
#include "dZoneManager.h"

// The zone table's storage (CDZoneTableTable.cpp), to add the test zone
namespace CDZoneTableTable {
	extern Table entries;
}

namespace {
	template<typename T>
	void Write(std::ostream& stream, const T& value) {
		stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	void WriteString8(std::ostream& stream, const std::string& value) {
		Write<uint8_t>(stream, static_cast<uint8_t>(value.size()));
		stream.write(value.data(), value.size());
	}

	struct TestScene {
		std::string filename;
		uint32_t id = 0;
		uint32_t layer = 0;
	};

	// A zone file of a version from 31 on with the given scenes and no transitions or paths
	std::string WriteZone(uint32_t version, const std::vector<TestScene>& scenes) {
		std::ostringstream stream;
		Write(stream, version);
		if (version >= 36) Write<uint32_t>(stream, 1); // revision
		Write<uint32_t>(stream, 1);
		if (version >= 38) {
			Write(stream, NiPoint3(0.0f, 0.0f, 0.0f)); // spawn position
			for (const float value : { 1.0f, 0.0f, 0.0f, 0.0f }) Write(stream, value); // spawn rotation
		}
		if (version >= 38) Write<uint32_t>(stream, static_cast<uint32_t>(scenes.size()));
		else Write<uint8_t>(stream, static_cast<uint8_t>(scenes.size()));
		for (const auto& scene : scenes) {
			WriteString8(stream, scene.filename);
			if (version >= 33) {
				Write(stream, scene.id);
				Write(stream, scene.layer);
				WriteString8(stream, scene.filename);
				for (const uint8_t color : { 1, 2, 3 }) Write(stream, color);
			}
		}
		Write<uint8_t>(stream, 0); // boundary lines
		WriteString8(stream, "test.raw");
		WriteString8(stream, "Test Zone");
		WriteString8(stream, "A zone for tests");
		if (version >= 32) Write<uint32_t>(stream, 0); // transitions
		if (version >= 35) {
			Write<uint32_t>(stream, 8); // path chunk length
			Write<uint32_t>(stream, 1); // path chunk version
			Write<uint32_t>(stream, 0); // paths
		}
		return stream.str();
	}
}

// Loads a written .luz through the zone manager
class ZoneFileVersionTest : public GameDependenciesTest {
protected:
	static constexpr LWOMAPID TEST_MAP = 9993;
	std::filesystem::path root;

	void SetUp() override {
		SetUpDependencies();
		root = std::filesystem::temp_directory_path() / (std::string("dlu_zone_version_test_") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
		std::filesystem::create_directories(root / "res" / "maps" / "testzone");
		std::ofstream(root / "res" / "CDServer.sqlite").close(); // marks an unpacked client
		Game::assetManager = new AssetManager(root);

		CDZoneTable zone{};
		zone.zoneID = TEST_MAP;
		zone.zoneName = "testzone/test.luz";
		CDZoneTableTable::entries[TEST_MAP] = zone;
	}

	void TearDown() override {
		CDZoneTableTable::entries.erase(TEST_MAP);
		delete Game::assetManager;
		Game::assetManager = nullptr;
		std::filesystem::remove_all(root);
		TearDownDependencies();
	}

	Zone* LoadZone(const std::string& data, Zone::SceneTableLookup lookup = nullptr) const {
		std::ofstream(root / "res" / "maps" / "testzone" / "test.luz", std::ios::binary).write(data.data(), data.size());
		Game::zoneManager->LoadZone(LWOZONEID(TEST_MAP, 0, 0));
		Zone* zone = Game::zoneManager->GetZoneMut();
		if (lookup) zone->SetSceneTableLookup(std::move(lookup));
		zone->LoadZoneIntoMemory();
		return zone;
	}

	static std::vector<std::pair<LWOSCENEID, std::string>> Scenes(const Zone* zone) {
		std::vector<std::pair<LWOSCENEID, std::string>> scenes;
		for (const auto& [id, scene] : zone->GetScenes()) scenes.emplace_back(id, scene.filename);
		return scenes;
	}
};

// Until version 33 a zone file has no scene IDs: each scene's ID is its index in the file
TEST_F(ZoneFileVersionTest, OldScenesAreNumberedByTheirIndex) {
	const Zone* zone = LoadZone(WriteZone(31, { { "a.lvl" }, { "b.lvl" } }));
	const std::vector<std::pair<LWOSCENEID, std::string>> expected = { { LWOSCENEID(0, 0), "a.lvl" }, { LWOSCENEID(1, 0), "b.lvl" } };
	EXPECT_EQ(Scenes(zone), expected);
	EXPECT_EQ(zone->GetZoneName(), "Test Zone");
}

// A later scene with the same ID and layer replaces an earlier one; a scene ID without a layer 0 scene isn't loaded,
// and before version 41 only the layer 0 scenes are
TEST_F(ZoneFileVersionTest, KeepsTheScenesTheClientLoads) {
	const std::vector<TestScene> scenes = {
		{ "first.lvl", 1, 0 }, { "audio.lvl", 1, 1 }, { "orphan_audio.lvl", 2, 1 }, { "second.lvl", 1, 0 }, { "three.lvl", 3, 0 },
	};
	const std::vector<std::pair<LWOSCENEID, std::string>> latest = {
		{ LWOSCENEID(1, 0), "second.lvl" }, { LWOSCENEID(1, 1), "audio.lvl" }, { LWOSCENEID(3, 0), "three.lvl" },
	};
	EXPECT_EQ(Scenes(LoadZone(WriteZone(41, scenes))), latest);

	const std::vector<std::pair<LWOSCENEID, std::string>> older = { { LWOSCENEID(1, 0), "second.lvl" }, { LWOSCENEID(3, 0), "three.lvl" } };
	EXPECT_EQ(Scenes(LoadZone(WriteZone(40, scenes))), older);
}

// Before version 30 a scene is a SceneTable ID: in ID order, each one with a row gets the next index as its ID and the
// row's scene name as its file; the others aren't loaded. A file older than version 20 is read as 20
TEST_F(ZoneFileVersionTest, OldestScenesComeFromTheSceneTable) {
	for (const uint32_t version : { 25u, 10u }) {
		std::ostringstream stream;
		Write(stream, version);
		Write<uint32_t>(stream, 1); // world ID
		Write<uint8_t>(stream, 3);
		for (const uint32_t sceneTableID : { 7u, 9u, 3u }) Write(stream, sceneTableID);
		Write<uint8_t>(stream, 0); // boundary lines
		WriteString8(stream, "test.raw");
		Write<uint16_t>(stream, 0); // trailing bytes: older readers read a name and description here
		const Zone* zone = LoadZone(stream.str(), [](uint32_t sceneTableID) -> std::optional<std::string> {
			if (sceneTableID == 3) return "three.lvl";
			if (sceneTableID == 7) return "seven.lvl";
			return std::nullopt;
		});

		const std::vector<std::pair<LWOSCENEID, std::string>> expected = { { LWOSCENEID(0, 0), "three.lvl" }, { LWOSCENEID(1, 0), "seven.lvl" } };
		EXPECT_EQ(Scenes(zone), expected) << version;
		EXPECT_EQ(zone->GetZoneRawPath(), "test.raw") << version;
	}
}
