#pragma once
#include "CppScripts.h"

class PropertyDevice : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnRebuildComplete(Entity* self, Entity* target) override;
	const std::u16string m_PropertyOwnerVariable = u"PropertyOwnerID";
	const uint32_t m_PropertyMissionID = 1291;
};
