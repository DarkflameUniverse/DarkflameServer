#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>


#include "Manifest.h"

#include "Pack.h"
#include "PackIndex.h"

enum class eAssetBundleType {
	None,
	Unpacked,
	Packed
};

class AssetManager {
public:
	AssetManager(const std::string& path);
	~AssetManager() = default;

	void GetFile(char* name, char** data, uint32_t* len);

private:
	void LoadManifest(const std::string& name);
	void LoadPackIndex();
	void UnpackRequiredAssets();

	// Modified crc algorithm (mpeg2)
	// Reference: https://stackoverflow.com/questions/54339800/how-to-modify-crc-32-to-crc-32-mpeg-2
	uint32_t crc32b(uint32_t base, uint8_t* message, size_t l) {
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

	bool m_SuccessfullyLoaded;

	std::filesystem::path m_Path;

	eAssetBundleType m_AssetBundleType = eAssetBundleType::None;

	PackIndex* m_PackIndex;
	std::unordered_map<std::string, Manifest*> m_Manifests;
};
