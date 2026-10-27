#pragma once

// Custom Classes
#include "CDTable.h"

struct CDSceneTable {
	uint32_t sceneID;                       //!< The scene's ID, as zone files before version 30 list their scenes
	std::string sceneName;                  //!< The scene's file
};

namespace CDSceneTableTable {
	using Table = std::map<uint32_t, CDSceneTable>;
	void LoadValuesFromDatabase();

	// Queries the table with a sceneID to find.
	const CDSceneTable* Query(uint32_t sceneID);
};
