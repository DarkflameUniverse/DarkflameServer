#pragma once
#include "CppScripts.h"
#include "BaseInteractDropLootServer.h"

class ImaginationShrineServer : public BaseInteractDropLootServer
{
public:
	void OnUse(Entity* self, Entity* user) override;
};

