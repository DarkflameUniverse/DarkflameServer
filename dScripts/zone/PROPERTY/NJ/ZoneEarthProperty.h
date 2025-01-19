#pragma once
#include "BasePropertyServer.h"

class ZoneEarthProperty : public BasePropertyServer {
	void SetGameVariables(Entity* self) override;
};
