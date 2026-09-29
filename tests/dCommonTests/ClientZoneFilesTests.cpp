#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "dCommonDependencies.h"
#include "LevelFile.h"
#include "MD5.h"
#include "Raw.h"
#include "Sd0.h"
#include "ZoneFile.h"

/**
 * Reads every zone (.luz), scene (.lvl) and terrain (.raw) file of the clients on disk: each must read in full, as it
 * does in the client. The clients are
 * looked for in DLU_CLIENTS_DIR (default ~/Documents/luclients); the tests are skipped when there are none.
 */

namespace {
	std::filesystem::path ClientsDir() {
		if (const char* dir = std::getenv("DLU_CLIENTS_DIR")) return dir;
		if (const char* home = std::getenv("HOME")) return std::filesystem::path(home) / "Documents" / "luclients";
		return {};
	}

	// Files on disk that are damaged themselves, not misread, by the MD5 of their bytes (only checked for a file that
	// does not read)
	const std::set<std::string> DAMAGED_FILES = {
		"72895c754d511a92335abae03400ac0b", // a scene file that claims 81 objects but ends after the first (the client would read past its end)
		// Version 30 terrain files with a width x width scene map per chunk, where the client reads one byte (RAWReadSceneMap)
		// and so misreads every chunk after the first
		"4a0a298889a5328a5622fe02fb82e9ef",
		"cc424f50fd0388ce3902ae2a597e8a06",
		"4db9d07aa298a313cd277300aaac1691",
		"b43624b9d42311bc41765082bb2da60a",
		"c8d496250bd438ebcf934a40d73bf675",
		"e613163197144246e086851452e2550b",
	};

	bool IsDamaged(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		const std::string bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
		return DAMAGED_FILES.contains(MD5(bytes).hexdigest());
	}

	std::vector<std::filesystem::path> FilesWithExtension(const std::string& extension) {
		std::vector<std::filesystem::path> files;
		const auto dir = ClientsDir();
		std::error_code error;
		if (dir.empty() || !std::filesystem::is_directory(dir, error)) return files;
		const auto options = std::filesystem::directory_options::skip_permission_denied;
		for (auto it = std::filesystem::recursive_directory_iterator(dir, options, error); !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
			// An empty file is not one the client can read either
			if (!it->is_regular_file(error) || it->file_size(error) == 0) continue;
			auto fileExtension = it->path().extension().string();
			std::ranges::transform(fileExtension, fileExtension.begin(), ::tolower);
			if (fileExtension == extension) files.push_back(it->path());
		}
		std::ranges::sort(files);
		return files;
	}

	// The file's bytes, uncompressed when it is sd0. Some clients' loose sd0 files hold what followed the file in its
	// pack: the file is the first sd0 stream, up to the first chunk that does not fit
	std::string Contents(const std::filesystem::path& path) {
		std::error_code error;
		std::string data(std::filesystem::file_size(path, error), '\0');
		std::ifstream file(path, std::ios::binary);
		file.read(data.data(), static_cast<std::streamsize>(data.size()));
		if (!data.starts_with(std::string(Sd0::SD0_HEADER, 5))) return data;
		size_t end = 5;
		while (end + 4 <= data.size()) {
			uint32_t size = 0;
			std::memcpy(&size, data.data() + end, sizeof(size));
			if (size > data.size() - end - 4) break;
			end += 4 + size;
		}
		std::istringstream compressed(data.substr(0, end));
		return Sd0(compressed).GetAsStringUncompressed();
	}

	// A file in `dir` by its name, whatever its case (zone files name their terrain in any case)
	std::optional<std::filesystem::path> FindFile(const std::filesystem::path& dir, const std::string& name) {
		auto lower = [](std::string text) { std::ranges::transform(text, text.begin(), ::tolower); return text; };
		std::error_code error;
		for (const auto& entry : std::filesystem::directory_iterator(dir, error)) {
			if (lower(entry.path().filename().string()) == lower(name)) return entry.path();
		}
		return std::nullopt;
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
		std::istringstream stream(Contents(path));
		ZoneFile zone;
		bool threw = false;
		try { zone.Read(stream); } catch (const std::runtime_error&) { threw = true; }
		if ((threw || stream.fail() || zone.scenes.empty()) && IsDamaged(path)) continue;
		EXPECT_FALSE(threw) << path;
		EXPECT_FALSE(stream.fail()) << path;
		EXPECT_FALSE(zone.scenes.empty()) << path;
	}
}

// The terrain of every zone of version 30 or newer (older zones have terrain of an older format the server does not
// read, see Zone::LoadSceneMap)
TEST_F(ClientZoneFilesTests, EveryTerrainFileReads) {
	const auto files = FilesWithExtension(".luz");
	if (files.empty()) GTEST_SKIP() << "No client zone files under " << ClientsDir();

	// Many clients ship the same terrain; each is read once (by its name and size)
	std::set<std::pair<std::string, uintmax_t>> read;
	for (const auto& path : files) {
		std::istringstream stream(Contents(path));
		ZoneFile zone;
		zone.ReadHeader(stream);
		if (zone.fileFormatVersion < ZoneFile::FileFormatVersion::PrePreAlpha || zone.zoneRawPath.empty()) continue;
		const auto rawPath = FindFile(path.parent_path(), zone.zoneRawPath);
		if (!rawPath) continue; // not shipped with the zone
		std::error_code error;
		auto name = rawPath->filename().string();
		std::ranges::transform(name, name.begin(), ::tolower);
		if (!read.insert({ name, std::filesystem::file_size(*rawPath, error) }).second) continue;
		std::istringstream rawStream(Contents(*rawPath));
		Raw::Raw raw;
		EXPECT_TRUE(Raw::ReadRaw(rawStream, raw) || IsDamaged(*rawPath)) << *rawPath;
	}
}

TEST_F(ClientZoneFilesTests, EverySceneFileReads) {
	const auto files = FilesWithExtension(".lvl");
	if (files.empty()) GTEST_SKIP() << "No client scene files under " << ClientsDir();

	for (const auto& path : files) {
		std::istringstream stream(Contents(path));
		LevelFile level;
		bool threw = false;
		try { level.Read(stream); } catch (const std::runtime_error&) { threw = true; }
		EXPECT_TRUE(!threw || IsDamaged(path)) << path;
	}
}
