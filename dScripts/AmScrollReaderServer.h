#pragma once
#include "CppScripts.h"

class AmScrollReaderServer : public CppScripts::Script
{
public:
	void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) override;
};
