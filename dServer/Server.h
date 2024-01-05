#ifndef __SERVER__H__
#define __SERVER__H__

#include <string_view>

namespace Server {
	void SetupLogger(const std::string_view serviceName);
};

#endif  //!__SERVER__H__
