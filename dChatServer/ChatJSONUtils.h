#ifndef __CHATJSONUTILS_H__
#define __CHATJSONUTILS_H__

#include "json_fwd.hpp"
#include "PlayerContainer.h"

void to_json(nlohmann::json& data, const PlayerData& playerData);
void to_json(nlohmann::json& data, const PlayerContainer& playerContainer);
void to_json(nlohmann::json& data, const TeamContainer& teamData);
void to_json(nlohmann::json& data, const TeamData& teamData);

#endif // __CHATJSONUTILS_H__