#pragma once
#include "CppScripts.h"

class InstanceExitTransferPlayerToLastNonInstance : public CppScripts::Script
{
public:
	void OnUse(Entity* self, Entity* user) override;
	void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) override;
};
