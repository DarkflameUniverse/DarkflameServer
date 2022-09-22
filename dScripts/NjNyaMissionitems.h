#pragma once
#include "NPCAddRemoveItem.h"
#include <map>
#include <vector>

class NjNyaMissionitems : public NPCAddRemoveItem {
	std::map<uint32_t, std::vector<ItemSetting>> GetSettings() override;
};
