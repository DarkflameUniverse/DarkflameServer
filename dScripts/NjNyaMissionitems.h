#pragma once
#include "NPCAddRemoveItem.h"

class NjNyaMissionitems : public NPCAddRemoveItem {
    std::map<uint32_t, std::vector<ItemSetting>> GetSettings() override;
};
