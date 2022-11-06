#pragma once
#include "NtFactionSpyServer.h"

class NtHaelServer : public NtFactionSpyServer {
	void SetVariables(Entity* self) override;
};
