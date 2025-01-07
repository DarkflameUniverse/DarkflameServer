#ifndef __JSONUTILS_H__
#define __JSONUTILS_H__

#include "json_fwd.hpp"
#include "PlayerContainer.h"
#include "eHTTPStatusCode.h"

struct HTTPReply {
	eHTTPStatusCode status = eHTTPStatusCode::NOT_FOUND;
	std::string message = "{\"error\":\"Not Found\"}";
};

void to_json(nlohmann::json& data, const PlayerData& playerData);
void to_json(nlohmann::json& data, const PlayerContainer& playerContainer);
void to_json(nlohmann::json& data, const TeamContainer& teamData);
void to_json(nlohmann::json& data, const TeamData& teamData);

namespace JSONUtils {
	// check required data for reqeust
	std::string CheckRequiredData(const nlohmann::json& data, const std::vector<std::string>& requiredData);
	bool Validate(const std::optional<nlohmann::json>& data, HTTPReply& reply);
}

#endif // __JSONUTILS_H__
