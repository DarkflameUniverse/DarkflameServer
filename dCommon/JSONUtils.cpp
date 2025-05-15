#include "JSONUtils.h"
#include "json.hpp"

using json = nlohmann::json;

std::string JSONUtils::CheckRequiredData(const json& data, const std::vector<std::string>& requiredData) {
	json check;
	check["error"] = json::array();
	for (const auto& required : requiredData) {
		if (!data.contains(required)) {
			check["error"].push_back("Missing Parameter: " + required);
		} else if (data[required] == "") {
			check["error"].push_back("Empty Parameter: " + required);
		}
	}
	return check["error"].empty() ? "" : check.dump();
}
