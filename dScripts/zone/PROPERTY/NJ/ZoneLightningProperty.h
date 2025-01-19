#pragma once
#include "BasePropertyServer.h"

class ZoneLightningProperty : public BasePropertyServer {
	void SetGameVariables(Entity* self) override;
};
