#pragma once
#include "CppScripts.h"

class NjRailPostServer : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) override;
	void OnRebuildNotifyState(Entity* self, eRebuildState state) override;
private:
	Entity* GetRelatedRail(Entity* self);
	const std::u16string NetworkNotActiveVariable = u"NetworkNotActive";
	const std::u16string NotActiveVariable = u"NotActive";
	const std::u16string RailGroupVariable = u"RailGroup";
};
