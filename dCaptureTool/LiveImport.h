#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "CaptureBundle.h"

/**
 * Converts the 2014 live captures (folders of <name>_traffic.zip files, one packet per .bin entry named
 * "<n>_<from port>-<to port>[_<part>]_[<header bytes>]...bin") into packet bundles, so they replay like the server's
 * own captures (docs/CaptureReplay.md). Only those zips are read: raw or encrypted captures (.pcap, key files) are
 * left alone. Secrets are removed as when capturing (PacketDecoder::Redact), and the characters' own data
 * (CREATE_CHARACTER) becomes the setup section.
 */
namespace LiveImport {
	struct Result {
		CaptureBundle::Bundle bundle;
		size_t zips{};
		size_t packets{};
		size_t skipped{};   // entries that weren't packets, or secrets that didn't read
		std::string error;
	};

	// One scenario: a folder of *_traffic.zip (auth, char, world, world1, world2, ... in that order), or one zip
	Result Import(const std::filesystem::path& scenario);

	// Every folder under root that holds *_traffic.zip files
	std::vector<std::filesystem::path> FindScenarios(const std::filesystem::path& root);
}
