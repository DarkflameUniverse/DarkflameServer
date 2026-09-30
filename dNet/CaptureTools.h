#ifndef __CAPTURETOOLS__H__
#define __CAPTURETOOLS__H__

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "CaptureBundle.h"
#include "dCommonVars.h"
#include "json.hpp"

/**
 * What the dashboard's capture viewer and the capture tool do with recorded packets (docs/CaptureReplay.md):
 * describe them, order them on one timeline, pull the player's movement out, make a bundle portable or anonymous,
 * and compare a replay's answers with the recorded ones. Pure functions over records, so they are unit tested.
 */
namespace CaptureTools {
	// The records of all servers on one timeline: by time, then by server and its sequence
	void SortTimeline(std::vector<CaptureBundle::Record>& records);

	// Whether a record went from a game client to a server
	bool FromClient(const PacketRecordHeader& header);

	// One record for the viewer: where it went, its name and, when `fields`, its decoded fields
	nlohmann::json RecordJson(const CaptureBundle::Record& record, size_t index, int64_t startUs, bool fields);

	struct Track {
		LWOOBJID characterId{};
		uint32_t zoneId{};
		uint32_t instanceId{};
		std::vector<float> samples; // t (seconds from the capture's start), x, y, z, ...
	};
	// Where each captured character moved (their POSITION_UPDATEs), per zone and instance
	std::vector<Track> Tracks(const std::vector<CaptureBundle::Record>& records, int64_t startUs);

	struct WorldVisit {
		LWOOBJID characterId{};
		float t{}; // seconds from the capture's start: the character's first packet on this world server
		uint32_t zoneId{};
		uint32_t instanceId{};
		uint32_t cloneId{};
	};
	// Which world server each captured character was on, in time order: one entry each time the packets their client
	// sends move to another zone or instance (zone 0 is character select)
	std::vector<WorldVisit> Worlds(const std::vector<CaptureBundle::Record>& records, int64_t startUs);

	/**
	 * Makes a bundle portable: the source server's character and account IDs are replaced by placeholders
	 * (PLACEHOLDER_BASE + n, written in the records' bytes and headers), listed in meta.ids as "char#n" / "account#n";
	 * account names and session fields are already blank (they are never recorded). Returns the characters found, by
	 * symbol, with their source ID, for the setup section.
	 */
	constexpr int64_t PLACEHOLDER_BASE = 0x1FEDC00000000000LL;
	std::map<std::string, LWOOBJID> MakePortable(CaptureBundle::Bundle& bundle);

	// Blanks what players typed and names (chat text, character and account names) in every packet whose struct is
	// known, so a bundle can be kept as a test fixture. Returns how many packets were changed.
	size_t Anonymise(CaptureBundle::Bundle& bundle);

	/**
	 * A replay's answers against the recorded ones. Server->client packets are paired in order by name; paired
	 * packets are compared by their decoded fields, leaving out what legitimately differs between runs (object IDs
	 * the server makes, timestamps, session keys, instance and clone IDs, server addresses).
	 */
	struct DiffReport {
		size_t expected{};
		size_t matched{};      // same fields
		size_t differing{};    // same packet, different fields
		size_t missing{};      // recorded, not answered in the replay
		size_t extra{};        // answered in the replay, not recorded
		std::map<std::string, size_t> differingByName;
		std::map<std::string, size_t> missingByName;
		std::map<std::string, size_t> extraByName;
		std::vector<std::string> examples; // the first few differences, readable
		nlohmann::json ToJson() const;
	};
	DiffReport Diff(const std::vector<CaptureBundle::Record>& expected, const std::vector<CaptureBundle::Record>& actual);

	// Fields left out of comparisons (by name, in any packet)
	bool IsVolatileField(const std::string& name);
}

#endif  //!__CAPTURETOOLS__H__
