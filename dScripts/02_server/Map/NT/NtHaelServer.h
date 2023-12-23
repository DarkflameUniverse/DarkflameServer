#pragma once
#include "NtFactionSpyServer.h"
#include "NtBcSubmitServer.h"

class NtHaelServer : public NtFactionSpyServer, public NtBcSubmitServer {
	void SetVariables(Entity* self) override;
};
