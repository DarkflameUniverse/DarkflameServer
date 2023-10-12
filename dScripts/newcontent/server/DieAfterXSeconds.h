#pragma once
#include "CppScripts.h"

class DieAfterXSeconds : public CppScripts::Script
{
public:
	DieAfterXSeconds(uint32_t time) {
		m_Time = time;
	};
	void OnStartup(Entity* self) override;
private:
	uint32_t m_Time = 20;
};
