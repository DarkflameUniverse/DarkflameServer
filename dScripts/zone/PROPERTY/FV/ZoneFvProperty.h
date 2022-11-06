#pragma once
#include "BasePropertyServer.h"

class ZoneFvProperty : public BasePropertyServer {
	void SetGameVariables(Entity* self) override;
};
