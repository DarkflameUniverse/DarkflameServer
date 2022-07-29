#pragma once
#include "BasePropertyServer.h"

class ZoneNsMedProperty : public BasePropertyServer {
	void SetGameVariables(Entity* self) override;
};
