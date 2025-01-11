#ifndef __ISERVERS__H__
#define __ISERVERS__H__

#include <cstdint>
#include <optional>

class IServers {
public:
	struct MasterInfo {
		std::string ip;
		uint32_t port{};
		std::string password{};
	};

	// Set the master server ip and port.
	virtual void SetMasterInfo(const MasterInfo& info) = 0;

	// Get the master server info.
	virtual std::optional<MasterInfo> GetMasterInfo() = 0;
};

#endif  //!__ISERVERS__H__
