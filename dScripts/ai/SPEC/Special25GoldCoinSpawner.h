#ifndef __SPECIAL25GOLDCOINSPAWNER__H__
#define __SPECIAL25GOLDCOINSPAWNER__H__

#include "SpecialCoinSpawner.h"

class Special25GoldCoinSpawner : public SpecialCoinSpawner {
public:
	Special25GoldCoinSpawner() : SpecialCoinSpawner(m_currencyDenomination) {};
private:
	static const uint32_t m_currencyDenomination = 250000;
};

#endif  //!__SPECIAL25GOLDCOINSPAWNER__H__
