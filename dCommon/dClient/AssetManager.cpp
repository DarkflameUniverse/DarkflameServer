#include "AssetManager.h"

#include <zlib.h>

AssetManager::AssetManager(const std::string& path) {
	if (!std::filesystem::is_directory(path)) {
		Game::logger->Log("AssetManager", "Attempted to load asset bundle (%s) however it is not a valid directory. ", path.c_str());

		return;
	}

	m_Path = std::filesystem::path(path);

	if (std::filesystem::exists(m_Path / "client") && std::filesystem::exists(m_Path / "versions")) {
		m_AssetBundleType = eAssetBundleType::Packed;
	}

	if (std::filesystem::exists(m_Path / "client" / "res" / "cdclient.fdb") && !std::filesystem::exists(m_Path / "client" / "res" / "pack")) {
		m_AssetBundleType = eAssetBundleType::Unpacked;
	}

	if (m_AssetBundleType == eAssetBundleType::None) {
		Game::logger->Log("AssetManager", "Failed to identify client type, cannot read client data");

		return;
	}

	switch (m_AssetBundleType) {
		case eAssetBundleType::Packed: {
			this->LoadManifest("trunk.txt");
			this->LoadPackIndex();

			this->UnpackRequiredAssets();

			break;
		}
	}
}

void AssetManager::LoadManifest(const std::string& name) {
	Manifest* manifest = new Manifest(m_Path / "versions" / name);

	m_Manifests.insert({ name, manifest });
}

void AssetManager::LoadPackIndex() {
	m_PackIndex = new PackIndex(m_Path);
}

void AssetManager::GetFile(char* name, char** data, uint32_t* len) {
	auto fixedName = std::string(name);
	std::transform(fixedName.begin(), fixedName.end(), fixedName.begin(), [](uint8_t c) { return std::tolower(c); });
	std::replace(fixedName.begin(), fixedName.end(), '/', '\\');

	if (std::filesystem::exists(m_Path / fixedName)) {
		FILE* file;
		fopen_s(&file, (m_Path / fixedName).string().c_str(), "rb");
		fseek(file, 0, SEEK_END);
		*len = ftell(file);
		*data = (char*)malloc(*len);
		fseek(file, 0, SEEK_SET);
		fread(*data, sizeof(uint8_t), *len, file);
		fclose(file);

		return;
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
		Game::logger->Log("AssetManager", "Failed to find file %s in packs.", fixedName.c_str());
		return;
	}

	auto packs = this->m_PackIndex->GetPacks();
	auto* pack = packs.at(packIndex);

	bool success = pack->ReadFileFromPack(crc, data, len);

	if (!success) {
		std::cout << this->m_PackIndex->GetPackPaths().size() << " | " << this->m_PackIndex->GetPacks().size() << std::endl;
		auto packPath = this->m_PackIndex->GetPackPaths().at(packIndex);
		Game::logger->Log("AssetManager", "Failed to get %s from pack file %s", fixedName.c_str(), packPath.c_str());
	}
}

void AssetManager::UnpackRequiredAssets() {
	if (std::filesystem::exists(m_Path / "client" / "res" / "cdclient.fdb")) return;

	char* data;
	uint32_t size;

	this->GetFile("client/res/cdclient.fdb", &data, &size);

	if (!data) {
		return;
	}

	std::ofstream cdclientOutput(m_Path / "client" / "res" / "cdclient.fdb", std::ios::out | std::ios::binary);
	cdclientOutput.write(data, size);
	cdclientOutput.close();

	return;
}
