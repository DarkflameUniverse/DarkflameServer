#pragma once
#include "BasePropertyServer.h"

class ZoneIceProperty : public BasePropertyServer {
	void SetGameVariables(Entity* self) override;
};
