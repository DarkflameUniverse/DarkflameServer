#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "CaptureBundle.h"
#include "CaptureTools.h"
#include "json.hpp"

/**
 * Replays a bundle's client packets against a server with the fake client (docs/CaptureReplay.md) and compares the
 * server's answers with the recorded ones.
 *
 * The recording is split into connections (each starts with the client's VERSION_CONFIRM). The replay logs in on
 * the target's auth server itself when the recording has no login, and goes through character select itself when
 * the recording starts in a zone. Before a packet goes out, what must differ on the target is filled in: the
 * target account's name and password, the session key the target's auth gave, and the target's IDs for the
 * characters (the bundle's placeholders). Timing follows the recording (sped up, and never more than a few seconds
 * between packets), and the replay waits for the answers a real client waits for (the handshake, the character
 * list, the zone to load, the world to transfer to).
 */
namespace Replayer {
	struct Options {
		std::string host{ "127.0.0.1" };
		uint16_t authPort{};
		std::string username;
		std::string password;
		double speed{ 4.0 };
		uint32_t maxGapMs{ 3000 };
		// Bundle placeholder -> the target's ID (the characters the setup made)
		std::map<int64_t, int64_t> ids;
		int64_t character{}; // the target's ID of the bundle's first character (character select picks it)
	};

	struct Result {
		bool loggedIn{};
		size_t connections{};       // recorded connections
		size_t connectionsReached{}; // connections the replay got to
		size_t sent{};
		size_t received{};
		std::string stoppedAt;      // why it stopped early, if it did
		std::vector<std::string> notes;
		std::vector<CaptureBundle::Record> actual; // what the target answered, as records
		CaptureTools::DiffReport diff;
		nlohmann::json ToJson() const;
	};

	Result Replay(const CaptureBundle::Bundle& bundle, const Options& options);
}
