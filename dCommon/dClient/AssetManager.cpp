#include <filesystem>

#include "AssetManager.h"
#include "Game.h"
#include "Logger.h"

#include "zlib.h"

constexpr uint32_t CRC32_INIT = 0xFFFFFFFF;
constexpr auto NULL_TERMINATOR = std::string_view{ "\0\0\0", 4 };

AssetManager::AssetManager(const std::filesystem::path& path) {
	if (!std::filesystem::is_directory(path)) {
		throw std::runtime_error("Attempted to load asset bundle (" + path.string() + ") however it is not a valid directory.");
	}

	m_Path = path;

	if (std::filesystem::exists(m_Path / "client") && std::filesystem::exists(m_Path / "versions")) {
		m_AssetBundleType = eAssetBundleType::Packed;

		m_RootPath = m_Path;
		m_ResPath = (m_Path / "client" / "res");
	} else if (std::filesystem::exists(m_Path / "res" / "pack")) {
		if (!std::filesystem::exists(m_Path / ".." / "versions")) {
			throw std::runtime_error("No \"versions\" directory found in the parent directories of \"res\" - packed asset bundle cannot be loaded.");
		}

		m_AssetBundleType = eAssetBundleType::Packed;

		m_RootPath = (m_Path / "..");
		m_ResPath = (m_Path / "res");
	} else if (std::filesystem::exists(m_Path / "pack")) {
		if (!std::filesystem::exists(m_Path / ".." / ".." / "versions")) {
			throw std::runtime_error("No \"versions\" directory found in the parent directories of \"res\" - packed asset bundle cannot be loaded.");
		}

		m_AssetBundleType = eAssetBundleType::Packed;

		m_RootPath = (m_Path / ".." / "..");
		m_ResPath = m_Path;
	} else if ((std::filesystem::exists(m_Path / "res" / "cdclient.fdb") || std::filesystem::exists(m_Path / "res" / "CDServer.sqlite")) && !std::filesystem::exists(m_Path / "res" / "pack")) {
		m_AssetBundleType = eAssetBundleType::Unpacked;

		m_ResPath = (m_Path / "res");
	} else if ((std::filesystem::exists(m_Path / "cdclient.fdb") || std::filesystem::exists(m_Path / "CDServer.sqlite")) && !std::filesystem::exists(m_Path / "pack")) {
		m_AssetBundleType = eAssetBundleType::Unpacked;

		m_ResPath = m_Path;
	}

	if (m_AssetBundleType == eAssetBundleType::None) {
		throw std::runtime_error("Failed to identify client type, cannot read client data.");
	}

	switch (m_AssetBundleType) {
	case eAssetBundleType::Packed: {
		this->LoadPackIndex();
		break;
	}
	case eAssetBundleType::None:
		[[fallthrough]];
	case eAssetBundleType::Unpacked: {
		break;
	}
	}
}

void AssetManager::LoadPackIndex() {
	m_PackIndex = PackIndex(m_RootPath);
}

namespace {
	// The whole file, malloc'd as GetFile's callers free it
	bool ReadLooseFile(const std::filesystem::path& path, char** data, uint32_t* len) {
		FILE* file;
#ifdef _WIN32
		fopen_s(&file, path.string().c_str(), "rb");
#elif __APPLE__
		// macOS has 64bit file IO by default
		file = fopen(path.string().c_str(), "rb");
#else
		file = fopen64(path.string().c_str(), "rb");
#endif
		if (!file) return false;
		fseek(file, 0, SEEK_END);
		*len = ftell(file);
		*data = static_cast<char*>(malloc(*len));
		fseek(file, 0, SEEK_SET);
		const auto read = fread(*data, sizeof(uint8_t), *len, file);
		fclose(file);
		if (read != *len) {
			free(*data);
			*data = nullptr;
			return false;
		}
		return true;
	}
}

void AssetManager::SetOverrideDir(const std::filesystem::path& dir) {
	m_OverrideDir = dir;
}

std::optional<std::string> AssetManager::OverrideName(std::string name) {
	std::transform(name.begin(), name.end(), name.begin(), [](uint8_t c) { return std::tolower(c); });
	std::replace(name.begin(), name.end(), '\\', '/');
	if (!name.starts_with("maps/") || name.find('\0') != std::string::npos) return std::nullopt;
	size_t start = 0;
	while (start <= name.size()) {
		const auto end = std::min(name.find('/', start), name.size());
		const auto part = std::string_view(name).substr(start, end - start);
		if (part.empty() || part == "." || part == "..") return std::nullopt;
		start = end + 1;
	}
	return name;
}

std::optional<std::filesystem::path> AssetManager::GetOverridePath(std::string name) const {
	if (m_OverrideDir.empty()) return std::nullopt;
	const auto relative = OverrideName(std::move(name));
	if (!relative) return std::nullopt;
	const auto path = m_OverrideDir / *relative;
	std::error_code code;
	if (!std::filesystem::is_regular_file(path, code)) return std::nullopt;
	return path;
}

bool AssetManager::HasFile(std::string name) const {
	return HasFile(std::move(name), true);
}

bool AssetManager::HasClientFile(const std::string& name) const {
	return HasFile(name, false);
}

bool AssetManager::GetFile(std::string name, char** data, uint32_t* len) const {
	return GetFile(std::move(name), data, len, true);
}

AssetStream AssetManager::GetClientFile(const std::string& name) const {
	char* buf = nullptr;
	uint32_t len = 0;
	const bool success = GetFile(name, &buf, &len, false);
	return AssetStream(buf, len, success);
}

bool AssetManager::HasFile(std::string fixedName, const bool useOverride) const {
	if (useOverride && GetOverridePath(fixedName)) return true;
	std::transform(fixedName.begin(), fixedName.end(), fixedName.begin(), [](uint8_t c) { return std::tolower(c); });

	// Special case for unpacked client have BrickModels in upper case
	if (this->m_AssetBundleType == eAssetBundleType::Unpacked) GeneralUtils::ReplaceInString(fixedName, "brickmodels", "BrickModels");

	std::replace(fixedName.begin(), fixedName.end(), '\\', '/');
	if (std::filesystem::exists(m_ResPath / fixedName)) return true;

	if (this->m_AssetBundleType == eAssetBundleType::Unpacked || !m_PackIndex) return false;

	std::replace(fixedName.begin(), fixedName.end(), '/', '\\');
	if (fixedName.rfind("client\\res\\", 0) != 0) fixedName = "client\\res\\" + fixedName;

	const auto crc = crc32b(crc32b(CRC32_INIT, fixedName), NULL_TERMINATOR);

	for (const auto& item : this->m_PackIndex->GetPackFileIndices()) {
		if (item.m_Crc == crc) {
			return true;
		}
	}

	return false;
}

bool AssetManager::GetFile(std::string fixedName, char** data, uint32_t* len, const bool useOverride) const {
	if (useOverride) {
		if (const auto overridden = GetOverridePath(fixedName)) return ReadLooseFile(*overridden, data, len);
	}
	std::transform(fixedName.begin(), fixedName.end(), fixedName.begin(), [](uint8_t c) { return std::tolower(c); });
	std::replace(fixedName.begin(), fixedName.end(), '\\', '/'); // On the off chance someone has the wrong slashes, force forward slashes

	// Special case for unpacked client have BrickModels in upper case
	if (this->m_AssetBundleType == eAssetBundleType::Unpacked) GeneralUtils::ReplaceInString(fixedName, "brickmodels", "BrickModels");

	if (std::filesystem::exists(m_ResPath / fixedName)) {
		return ReadLooseFile(m_ResPath / fixedName, data, len);
	}

	if (this->m_AssetBundleType == eAssetBundleType::Unpacked) return false;

	// The crc in side of the pack always uses backslashes, so we need to convert them again...
	std::replace(fixedName.begin(), fixedName.end(), '/', '\\');
	if (fixedName.rfind("client\\res\\", 0) != 0) {
		fixedName = "client\\res\\" + fixedName;
	}
	int32_t packIndex = -1;
	auto crc = crc32b(crc32b(CRC32_INIT, fixedName), NULL_TERMINATOR);

	for (const auto& item : this->m_PackIndex->GetPackFileIndices()) {
		if (item.m_Crc == crc) {
			packIndex = item.m_PackFileIndex;
			crc = item.m_Crc;
			break;
		}
	}

	if (packIndex == -1 || !crc) {
		return false;
	}

	const auto& pack = this->m_PackIndex->GetPacks().at(packIndex);
	bool success = false;
	try {
		success = pack.ReadFileFromPack(crc, data, len);
	} catch (std::exception& e) {
		LOG("Failed to read file %s from pack file", fixedName.c_str());
	}
	return success;
}

std::optional<std::filesystem::path> AssetManager::GetLoosePath(std::string fixedName) const {
	if (auto overridden = GetOverridePath(fixedName)) return overridden;
	// The same name fixing as GetFile
	std::transform(fixedName.begin(), fixedName.end(), fixedName.begin(), [](uint8_t c) { return std::tolower(c); });
	std::replace(fixedName.begin(), fixedName.end(), '\\', '/');
	if (this->m_AssetBundleType == eAssetBundleType::Unpacked) GeneralUtils::ReplaceInString(fixedName, "brickmodels", "BrickModels");
	const auto path = m_ResPath / fixedName;
	std::error_code code;
	if (!std::filesystem::exists(path, code)) return std::nullopt;
	return path;
}

AssetStream AssetManager::GetFile(const char* name) const {
	char* buf; uint32_t len;

	bool success = this->GetFile(name, &buf, &len);

	return AssetStream(buf, len, success);
}

uint32_t AssetManager::crc32b(uint32_t crc, const std::string_view message) {
	for (const auto byte : message) {
		// xor next byte to upper bits of crc
		crc ^= (static_cast<uint32_t>(std::bit_cast<uint8_t>(byte)) << 24);
		for (size_t _ = 0; _ < 8; _++) { // Do eight times.
			const uint32_t msb = crc >> 31;
			crc <<= 1;
			crc ^= (0 - msb) & 0x04C11DB7;
		}
	}
	return crc; // don't complement crc on output
}
