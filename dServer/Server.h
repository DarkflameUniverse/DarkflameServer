#ifndef __SERVER__H__
#define __SERVER__H__

#include <string_view>

namespace Server {
	// takes in an optional argument of folder should you want to place the logs in a sub-folder
	void SetupLogger(const std::string_view serviceName, const std::string_view folder = "");
};

#endif  //!__SERVER__H__
