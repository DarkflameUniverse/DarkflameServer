#pragma once
#include "NjRailActivatorsServer.h"

class NjIceRailActivator : public NjRailActivatorsServer {
	void OnPlayerRailArrived(Entity* self, Entity* sender, const std::u16string& pathName, int32_t waypoint) override;
private:
	std::u16string BreakpointVariable = u"BreakPoint";
	std::u16string BlockGroupVariable = u"BlockGroup";
	std::u16string IceBlockVariable = u"IceBlock";
};
