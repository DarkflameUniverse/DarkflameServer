#pragma once
#include "CppScripts.h"

// L_FIRE_TRANS_ROCKS_MANAGER.lua: the manager of the rocks in the NJ monastery's fire transition. Once all the rocks of
// its RockGroup are there (NumberOfRocks, 3) it sends them up 5 s later, and each time all of them are back down it
// sends them up again after 1 s.
class FireTransRocksManager : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	void CheckForRocks(Entity* self);
	void SendRocksUp(Entity* self);
};
