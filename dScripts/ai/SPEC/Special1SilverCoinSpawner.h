#ifndef __SPECIAL1SILVERCOINSPAWNER__H__
#define __SPECIAL1SILVERCOINSPAWNER__H__

#include "SpecialCoinSpawner.h"

class Special1SilverCoinSpawner : public SpecialCoinSpawner {
public:
	Special1SilverCoinSpawner() : SpecialCoinSpawner(m_currencyDenomination) {};
private:
	static const uint32_t m_currencyDenomination = 100;
};

#endif  //!__SPECIAL1SILVERCOINSPAWNER__H__
