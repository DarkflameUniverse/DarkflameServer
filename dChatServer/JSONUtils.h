#ifndef __JSONUTILS_H__
#define __JSONUTILS_H__

#include "json_fwd.hpp"
#include "PlayerContainer.h"
#include "TeamContainer.h"

/* Remember, to_json needs to be in the same namespace as the class its located in */

void to_json(nlohmann::json& data, const PlayerData& playerData);
void to_json(nlohmann::json& data, const PlayerContainer& playerContainer);
void to_json(nlohmann::json& data, const TeamData& teamData);

namespace TeamContainer {
	void to_json(nlohmann::json& data, const TeamContainer::Data& teamData);
};

namespace JSONUtils {
	// check required data for reqeust
	std::string CheckRequiredData(const nlohmann::json& data, const std::vector<std::string>& requiredData);
}

#endif // __JSONUTILS_H__
