#ifndef __SPECIALIMAGINEPOWERUPSPAWNER2PT__H__
#define __SPECIALIMAGINEPOWERUPSPAWNER2PT__H__

#include "SpecialPowerupSpawner.h"

class SpecialImaginePowerupSpawner2pt : public SpecialPowerupSpawner {
public:
	SpecialImaginePowerupSpawner2pt() : SpecialPowerupSpawner(m_SkillId) {};
private:
	uint32_t m_SkillId = 129;
};

#endif  //!__SPECIALIMAGINEPOWERUPSPAWNER2PT__H__
