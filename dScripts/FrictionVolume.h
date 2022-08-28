#pragma once
#include "CppScripts.h"

class FrictionVolume : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
private:
	const float m_DefaultFriction = 1.5;
	const std::u16string m_FrictionVariable = u"FrictionAmt";
};
