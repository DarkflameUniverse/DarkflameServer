#ifndef _JSONUTILS_H_
#define _JSONUTILS_H_

#include "json_fwd.hpp"

namespace JSONUtils {
	// check required fields in json data
	std::string CheckRequiredData(const nlohmann::json& data, const std::vector<std::string>& requiredData);
}

#endif // _JSONUTILS_H_
