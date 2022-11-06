#pragma once
#include "CppScripts.h"
class WblGenericZone : public CppScripts::Script
{
public:
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
private:
	const LWOMAPID m_WblMainZone = 1600;
	const std::string m_WblAbortMsg = "AbortWBLZone";
};
