#ifndef __SPECIAL1GOLDCOINSPAWNER__H__
#define __SPECIAL1GOLDCOINSPAWNER__H__

#include "SpecialCoinSpawner.h"

class Special1GoldCoinSpawner : public SpecialCoinSpawner {
public:
	Special1GoldCoinSpawner() : SpecialCoinSpawner(m_currencyDenomination) {};
private:
	static const uint32_t m_currencyDenomination = 10000;
};

#endif  //!__SPECIAL1GOLDCOINSPAWNER__H__
