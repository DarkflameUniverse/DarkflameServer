#pragma once
#include "CppScripts.h"

class FvFacilityPipes : public CppScripts::Script {
public:
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
private:
	const std::u16string m_EffectType = u"create";
	const std::string m_LeftPipeEffectName = "LeftPipeOff";
	const int32_t m_LeftPipeEffectID = 2774;
	const std::string m_RightPipeEffectName = "RightPipeOff";
	const int32_t m_RightPipeEffectID = 2777;
	const std::string m_ImaginationCanisterEffectName = "imagination_canister";
	const int32_t m_ImaginationCanisterEffectID = 2750;
};
