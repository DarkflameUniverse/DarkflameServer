#pragma once

#include <cstdint>

#include <string>
#include <vector>
#include <filesystem>

enum class eManifestSection {
	None,
	Version,
	Files
};

struct ManifestVersionInfo {
	uint32_t m_Version;
	std::string m_VersionHash;
	std::string m_HumanReadableVersion;
};

struct ManifestPath {
	std::string m_FileName;
	uint32_t m_FileSize;
	std::string m_FileHash;
	uint32_t m_CompressedFileSize;
	std::string m_CompressedFileHash;
	std::string m_IntegrityHash;
};

class Manifest {
public:
	Manifest(const std::filesystem::path& path);
	~Manifest() = default;

	const std::vector<ManifestPath>& GetPaths() { return m_Paths; }
private:
	std::ifstream m_FileStream;

	eManifestSection m_CurrentSection;

	ManifestVersionInfo m_Version;
	std::vector<ManifestPath> m_Paths;
};
