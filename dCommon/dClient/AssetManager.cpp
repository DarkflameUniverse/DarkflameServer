#include <filesystem>

#include "AssetManager.h"
#include "Game.h"
#include "Logger.h"

#include "zlib.h"

constexpr uint32_t CRC32_INIT = 0xFFFFFFFF;
constexpr auto NULL_TERMINATOR = std::string_view{"\0\0\0", 4};

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

bool AssetManager::HasFile(std::string fixedName) const {
	std::transform(fixedName.begin(), fixedName.end(), fixedName.begin(), [](uint8_t c) { return std::tolower(c); });

	// Special case for unpacked client have BrickModels in upper case
	if (this->m_AssetBundleType == eAssetBundleType::Unpacked) GeneralUtils::ReplaceInString(fixedName, "brickmodels", "BrickModels");

	std::replace(fixedName.begin(), fixedName.end(), '\\', '/');
	if (std::filesystem::exists(m_ResPath / fixedName)) return true;

	if (this->m_AssetBundleType == eAssetBundleType::Unpacked) return false;

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

bool AssetManager::GetFile(std::string fixedName, char** data, uint32_t* len) const {
	std::transform(fixedName.begin(), fixedName.end(), fixedName.begin(), [](uint8_t c) { return std::tolower(c); });
	std::replace(fixedName.begin(), fixedName.end(), '\\', '/'); // On the off chance someone has the wrong slashes, force forward slashes

	// Special case for unpacked client have BrickModels in upper case
	if (this->m_AssetBundleType == eAssetBundleType::Unpacked) GeneralUtils::ReplaceInString(fixedName, "brickmodels", "BrickModels");

	if (std::filesystem::exists(m_ResPath / fixedName)) {
		FILE* file;
#ifdef _WIN32
		fopen_s(&file, (m_ResPath / fixedName).string().c_str(), "rb");
#elif __APPLE__
		// macOS has 64bit file IO by default
		file = fopen((m_ResPath / fixedName).string().c_str(), "rb");
#else
		file = fopen64((m_ResPath / fixedName).string().c_str(), "rb");
#endif
		fseek(file, 0, SEEK_END);
		*len = ftell(file);
		*data = static_cast<char*>(malloc(*len));
		fseek(file, 0, SEEK_SET);
		int32_t readInData = fread(*data, sizeof(uint8_t), *len, file);
		fclose(file);

		return true;
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
	const bool success = pack.ReadFileFromPack(crc, data, len);

	return success;
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
