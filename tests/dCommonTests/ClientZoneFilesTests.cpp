#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "dCommonDependencies.h"
#include "LevelFile.h"
#include "Sd0.h"
#include "ZoneFile.h"

/**
 * Reads every zone (.luz) and scene (.lvl) file of the clients on disk: each must read in full. The clients are
 * looked for in DLU_CLIENTS_DIR (default ~/Documents/luclients); the tests are skipped when there are none.
 */

namespace {
	std::filesystem::path ClientsDir() {
		if (const char* dir = std::getenv("DLU_CLIENTS_DIR")) return dir;
		if (const char* home = std::getenv("HOME")) return std::filesystem::path(home) / "Documents" / "luclients";
		return {};
	}

	// Files on disk that are damaged themselves, not misread
	const std::set<std::string> DAMAGED_FILES = {
		"72895c754d511a92335abae03400ac0b", // MD5 of a scene file that claims 81 objects in 910 bytes
	};

	std::vector<std::filesystem::path> FilesWithExtension(const std::string& extension) {
		std::vector<std::filesystem::path> files;
		const auto dir = ClientsDir();
		std::error_code error;
		if (dir.empty() || !std::filesystem::is_directory(dir, error)) return files;
		const auto options = std::filesystem::directory_options::skip_permission_denied;
		for (auto it = std::filesystem::recursive_directory_iterator(dir, options, error); !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
			if (!it->is_regular_file(error) || it->file_size(error) == 0) continue;
			auto fileExtension = it->path().extension().string();
			std::ranges::transform(fileExtension, fileExtension.begin(), ::tolower);
			if (fileExtension == extension && !DAMAGED_FILES.contains(it->path().filename().string())) files.push_back(it->path());
		}
		std::ranges::sort(files);
		return files;
	}

	// The file's bytes; empty for sd0 files (the loose sd0 files of the 1.7.45 and 1.9.76 clients are all cut up,
	// and the server gets the files uncompressed from the packs)
	std::string Contents(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (data.starts_with(std::string(Sd0::SD0_HEADER, 5))) return {};
		return data;
	}
}

// The readers log what they skip
class ClientZoneFilesTests : public dCommonDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); Game::logger = nullptr; }
};

TEST_F(ClientZoneFilesTests, EveryZoneFileReads) {
	const auto files = FilesWithExtension(".luz");
	if (files.empty()) GTEST_SKIP() << "No client zone files under " << ClientsDir();

	for (const auto& path : files) {
		const auto data = Contents(path);
		if (data.empty()) continue;
		std::istringstream stream(data);
		ZoneFile zone;
		EXPECT_NO_THROW(zone.Read(stream)) << path;
		EXPECT_FALSE(stream.fail()) << path;
		EXPECT_FALSE(zone.scenes.empty()) << path;
	}
}

TEST_F(ClientZoneFilesTests, EverySceneFileReads) {
	const auto files = FilesWithExtension(".lvl");
	if (files.empty()) GTEST_SKIP() << "No client scene files under " << ClientsDir();

	for (const auto& path : files) {
		const auto data = Contents(path);
		if (data.empty()) continue;
		std::istringstream stream(data);
		LevelFile level;
		EXPECT_NO_THROW(level.Read(stream)) << path;
	}
}
