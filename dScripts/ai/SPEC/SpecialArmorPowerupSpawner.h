#ifndef __SPECIALARMORPOWERUPSPAWNER__H__
#define __SPECIALARMORPOWERUPSPAWNER__H__

#include "SpecialPowerupSpawner.h"

class SpecialArmorPowerupSpawner : public SpecialPowerupSpawner {
public:
	SpecialArmorPowerupSpawner() : SpecialPowerupSpawner(m_SkillId) {};
private:
	uint32_t m_SkillId = 80;
};


#endif  //!__SPECIALARMORPOWERUPSPAWNER__H__
