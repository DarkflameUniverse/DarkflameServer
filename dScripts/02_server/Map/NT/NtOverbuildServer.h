#pragma once
#include "NtFactionSpyServer.h"

class NtOverbuildServer : public NtFactionSpyServer {
	void SetVariables(Entity* self) override;
	const std::u16string m_OtherEntitiesGroupVariable = u"SpyConvo2Group";
};
