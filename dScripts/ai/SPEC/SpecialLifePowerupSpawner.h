#ifndef __SPECIALLIFEPOWERUPSPAWNER__H__
#define __SPECIALLIFEPOWERUPSPAWNER__H__

#include "SpecialPowerupSpawner.h"

class SpecialLifePowerupSpawner : public SpecialPowerupSpawner {
public:
	SpecialLifePowerupSpawner() : SpecialPowerupSpawner(m_SkillId) {};
private:
	uint32_t m_SkillId = 5;
};

#endif  //!__SPECIALLIFEPOWERUPSPAWNER__H__
