#pragma once
#include "NtFactionSpyServer.h"
#include "NtBcSubmitServer.h"

class NtOverbuildServer : public NtFactionSpyServer, public NtBcSubmitServer {
	void SetVariables(Entity* self) override;
	const std::u16string m_OtherEntitiesGroupVariable = u"SpyConvo2Group";
};
