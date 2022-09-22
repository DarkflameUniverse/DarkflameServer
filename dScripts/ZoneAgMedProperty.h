#pragma once
#include "BasePropertyServer.h"

class ZoneAgMedProperty : public BasePropertyServer {
	void SetGameVariables(Entity* self) override;
};
