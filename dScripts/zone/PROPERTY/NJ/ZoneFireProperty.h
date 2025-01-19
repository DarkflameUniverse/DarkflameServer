#pragma once
#include "BasePropertyServer.h"

class ZoneFireProperty : public BasePropertyServer {
	void SetGameVariables(Entity* self) override;
};
