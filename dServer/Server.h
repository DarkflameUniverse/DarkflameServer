#ifndef SERVER_H
#define SERVER_H

#include <string_view>

namespace Server {
	void SetupLogger(const std::string_view serviceName);
};

#endif  //!SERVER_H
