#ifndef __SERVER__H__
#define __SERVER__H__

#include <string_view>

class Server {
public:
	static void SetupLogger(const std::string_view serviceName);
};

#endif  //!__SERVER__H__
