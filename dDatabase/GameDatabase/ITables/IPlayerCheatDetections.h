#ifndef IPLAYERCHEATDETECTIONS_H
#define IPLAYERCHEATDETECTIONS_H

#include <cstdint>
#include <optional>

class IPlayerCheatDetections {
public:
	struct Info {
		std::optional<uint32_t> userId = std::nullopt;
		std::string username;
		std::string systemAddress;
		std::string extraMessage;
	};

	// Insert a new cheat detection.
	virtual void InsertCheatDetection(const IPlayerCheatDetections::Info& info) = 0;
};

#endif  //!IPLAYERCHEATDETECTIONS_H
