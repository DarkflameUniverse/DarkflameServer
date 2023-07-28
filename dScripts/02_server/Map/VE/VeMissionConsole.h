#pragma once
#include "CppScripts.h"

class VeMissionConsole : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user) override;
private:
	const std::u16string m_NumberVariable = u"num";
};
