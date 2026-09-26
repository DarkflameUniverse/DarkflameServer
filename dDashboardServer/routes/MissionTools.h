#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "dCommonVars.h"
#include "MissionXml.h"

/**
 * Missions on the character page: a character's missions and achievements with their names, tasks and state from
 * the game data, and staff completing, resetting or giving one. A character in game is changed by its world server
 * (ePlayerAction::MISSION_*), exactly as the GM commands do; one that isn't has its saved data changed the way the
 * game would save the same change (MissionXml).
 */
namespace MissionCatalog {
	struct TaskInfo {
		uint32_t uid{}; // locale key MissionTasks_<uid>_description
		eMissionTaskType type{ eMissionTaskType::UNKNOWN };
		int32_t target{};
		uint32_t targetValue{};
	};

	struct Info {
		uint32_t id{};
		std::string type;    // the game's grouping (defined_type / defined_subtype), as its journal and passport show them
		std::string subtype;
		bool released{};     // locStatus 2: localized and live in the client
		int64_t coins{};
		int32_t uscore{};
		int64_t reputation{};
		std::vector<std::pair<LOT, int32_t>> rewardItems;
		MissionXml::Definition definition;
		std::vector<TaskInfo> tasks;
	};

	// Every mission in the game data, read once
	const std::map<uint32_t, Info>& All();
	const Info* Find(uint32_t id);
	const MissionXml::Definition* Definition(uint32_t id);

	std::string Name(uint32_t id);
	std::string TaskText(uint32_t uid);
}

void RegisterMissionToolRoutes();
