#pragma once
#include "BasePropertyServer.h"

class ZoneNsProperty : public BasePropertyServer {
	void SetGameVariables(Entity* self) override;
};
