#pragma once
#include "CppScripts.h"

class VeBricksampleServer : public CppScripts::Script {
	void OnUse(Entity* self, Entity* user) override;
	const std::u16string m_LootVariable = u"Loot";
};
