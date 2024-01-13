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

	int32_t ID;
	std::string animationName;
	std::string iconFilename;
	int32_t locState;
	int32_t channel;
	bool locked;
	bool localize;
	std::string gateVersion;
};

class CDEmoteTableTable : public CDTable<CDEmoteTableTable> {
private:
	std::map<int, CDEmoteTable> entries;

public:
	void LoadValuesFromDatabase();
	// Returns an emote by ID
	CDEmoteTable* GetEmote(int32_t id);
};
