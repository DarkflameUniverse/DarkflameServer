#pragma once
#include "BasePropertyServer.h"

class ZoneGfProperty : public BasePropertyServer {
	void SetGameVariables(Entity* self) override;
};
