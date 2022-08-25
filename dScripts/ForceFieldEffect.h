#pragma once
#include "CppScripts.h"

class ForceFieldEffect : public CppScripts::Script
{
public:
	void OnCollisionPhantom(Entity* self, Entity* target) override;
private:
	const std::u16string m_EffectType = u"cast";
	const LWOOBJID m_EffectID = 3671;
	const std::string m_EffectName = "objects\\powerfence_bang\\powerfence_bang";
};
