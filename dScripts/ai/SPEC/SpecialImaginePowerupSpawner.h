#ifndef __SPECIALIMAGINEPOWERUPSPAWNER__H__
#define __SPECIALIMAGINEPOWERUPSPAWNER__H__

#include "SpecialPowerupSpawner.h"

class SpecialImaginePowerupSpawner : public SpecialPowerupSpawner {
public:
	SpecialImaginePowerupSpawner() : SpecialPowerupSpawner(m_SkillId) {};
private:
	uint32_t m_SkillId = 13;
};

#endif  //!__SPECIALIMAGINEPOWERUPSPAWNER__H__
