#include <filesystem>

#include "AssetManager.h"
#include "Game.h"
#include "dLogger.h"

#include <zlib.h>

AssetManager::AssetManager(const std::filesystem::path& path) {
	if (!std::filesystem::is_directory(path)) {
		throw std::runtime_error("Attempted to load asset bundle (" + path.string() + ") however it is not a valid directory.");
	}

	m_Path = path;

	if (std::filesystem::exists(m_Path / "client") && std::filesystem::exists(m_Path / "versions")) {
		m_AssetBundleType = eAssetBundleType::Packed;

		m_RootPath = m_Path;
		m_ResPath = (m_Path / "client" / "res");
	} else if (std::filesystem::exists(m_Path / ".." / "versions") && std::filesystem::exists(m_Path / "res")) {
		m_AssetBundleType = eAssetBundleType::Packed;

		m_RootPath = (m_Path / "..");
		m_ResPath = (m_Path / "res");
	} else if (std::filesystem::exists(m_Path / "pack") && std::filesystem::exists(m_Path / ".." / ".." / "versions")) {
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
		case eAssetBundleType::Unpacked: {
			break;
		}
	}
}

void AssetManager::LoadPackIndex() {
	m_PackIndex = new PackIndex(m_RootPath);
}

std::filesystem::path AssetManager::GetResPath() {
	return m_ResPath;
}

eAssetBundleType AssetManager::GetAssetBundleType() {
	return m_AssetBundleType;
}

bool AssetManager::HasFile(const char* name) {
	auto fixedName = std::string(name);
	std::transform(fixedName.begin(), fixedName.end(), fixedName.begin(), [](uint8_t c) { return std::tolower(c); });

	// Special case for unpacked client have BrickModels in upper case
	if (this->m_AssetBundleType == eAssetBundleType::Unpacked) GeneralUtils::ReplaceInString(fixedName, "brickmodels", "BrickModels");

	std::replace(fixedName.begin(), fixedName.end(), '\\', '/');
	if (std::filesystem::exists(m_ResPath / fixedName)) return true;

	if (this->m_AssetBundleType == eAssetBundleType::Unpacked) return false;

	std::replace(fixedName.begin(), fixedName.end(), '/', '\\');
	if (fixedName.rfind("client\\res\\", 0) != 0) fixedName = "client\\res\\" + fixedName;

	uint32_t crc = crc32b(0xFFFFFFFF, (uint8_t*)fixedName.c_str(), fixedName.size());
	crc = crc32b(crc, (Bytef*)"\0\0\0\0", 4);

	for (const auto& item : this->m_PackIndex->GetPackFileIndices()) {
		if (item.m_Crc == crc) {
			return true;
		}
	}

	return false;
}

bool AssetManager::GetFile(const char* name, char** data, uint32_t* len) {
	auto fixedName = std::string(name);
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
		*data = (char*)malloc(*len);
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
	uint32_t crc = crc32b(0xFFFFFFFF, (uint8_t*)fixedName.c_str(), fixedName.size());
	crc = crc32b(crc, (Bytef*)"\0\0\0\0", 4);

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

	auto packs = this->m_PackIndex->GetPacks();
	auto* pack = packs.at(packIndex);

	bool success = pack->ReadFileFromPack(crc, data, len);

	return success;
}

AssetMemoryBuffer AssetManager::GetFileAsBuffer(const char* name) {
	char* buf;
	uint32_t len;

	bool success = this->GetFile(name, &buf, &len);

	return AssetMemoryBuffer(buf, len, success);
}

uint32_t AssetManager::crc32b(uint32_t base, uint8_t* message, size_t l) {
	size_t i, j;
	uint32_t crc, msb;

	crc = base;
	for (i = 0; i < l; i++) {
		// xor next byte to upper bits of crc
		crc ^= (((unsigned int)message[i]) << 24);
		for (j = 0; j < 8; j++) { // Do eight times.
			msb = crc >> 31;
			crc <<= 1;
			crc ^= (0 - msb) & 0x04C11DB7;
		}
	}
	return crc; // don't complement crc on output
}

AssetManager::~AssetManager() {
	delete m_PackIndex;
}
