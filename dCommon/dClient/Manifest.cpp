#include "Manifest.h"

#include "GeneralUtils.h"

Manifest::Manifest(const std::filesystem::path& path) {
	m_FileStream = std::ifstream(path, std::ios::in);

	std::string line;

	while (std::getline(m_FileStream, line)) {
		if (line[0] == '[') {
			// Change of section
			GeneralUtils::ReplaceInString(line, "[", "");
			GeneralUtils::ReplaceInString(line, "]", "");

			if (line == "version") {
				m_CurrentSection = eManifestSection::Version;
			} else if (line == "files") {
				m_CurrentSection = eManifestSection::Files;
			}

			continue;
		}

		switch (m_CurrentSection) {
			case eManifestSection::Version: {
				auto parts = GeneralUtils::SplitString(line, ',');

				if (!parts.size() == 3) {
					break; // Should never happen but don't want to cause a segfault
				}

				m_Version = ManifestVersionInfo();

				m_Version.m_Version = GeneralUtils::Parse<uint32_t>(parts[0]);
				m_Version.m_VersionHash = parts[1];
				m_Version.m_HumanReadableVersion = parts[2];

				break;
			}

			case eManifestSection::Files: {
				auto parts = GeneralUtils::SplitString(line, ',');

				if (!parts.size() == 6) {
					break; // Should never happen but don't want to cause a segfault
				}

				auto fileData = ManifestPath();

				fileData.m_FileName = parts[0];
				fileData.m_FileSize = GeneralUtils::Parse<uint32_t>(parts[1]);
				fileData.m_FileHash = parts[2];
				fileData.m_CompressedFileSize = GeneralUtils::Parse<uint32_t>(parts[3]);
				fileData.m_CompressedFileHash = parts[4];
				fileData.m_IntegrityHash = parts[5];

				m_Paths.push_back(fileData);
			}
		}
	}

	Game::logger->Log("Manifest", "Loaded client version %s with %i files", m_Version.m_HumanReadableVersion.c_str(), m_Paths.size());
	
	printf("");
}
