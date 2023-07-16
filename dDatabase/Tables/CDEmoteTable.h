#pragma once

// Custom Classes
#include "CDTable.h"
#include <map>

struct CDEmoteTable {
	CDEmoteTable() {
		ID = -1;
		animationName = "";
		iconFilename = "";
		locState = -1;
		channel = -1;
		locked = false;
		localize = false;
		gateVersion = "";
	}

	int ID;
	std::string animationName;
	std::string iconFilename;
	int locState;
	int channel;
	bool locked;
	bool localize;
	std::string gateVersion;
};

namespace CDEmoteTableTable {
	void LoadTableIntoMemory();
	// Returns an emote by ID
	CDEmoteTable* GetEmote(int id);
};
