#ifndef ISERVERS_H
#define ISERVERS_H

#include <cstdint>
#include <optional>

class IServers {
public:
	struct MasterInfo {
		std::string ip;
		uint32_t port{};
	};

	// Set the master server ip and port.
	virtual void SetMasterIp(const std::string_view ip, const uint32_t port) = 0;

	// Get the master server info.
	virtual std::optional<MasterInfo> GetMasterInfo() = 0;
};

#endif  //!ISERVERS_H
