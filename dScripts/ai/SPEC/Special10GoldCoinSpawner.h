#ifndef __SPECIAL10GOLDCOINSPAWNER__H__
#define __SPECIAL10GOLDCOINSPAWNER__H__

#include "SpecialCoinSpawner.h"

class Special10GoldCoinSpawner : public SpecialCoinSpawner {
public:
	Special10GoldCoinSpawner() : SpecialCoinSpawner(m_currencyDenomination) {};
private:
	static const uint32_t m_currencyDenomination = 100000;
};

#endif  //!__SPECIAL10GOLDCOINSPAWNER__H__
