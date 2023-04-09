#ifndef __SPECIAL10BRONZECOINSPAWNER__H__
#define __SPECIAL10BRONZECOINSPAWNER__H__

#include "SpecialCoinSpawner.h"

class Special10BronzeCoinSpawner : public SpecialCoinSpawner {
public:
	Special10BronzeCoinSpawner() : SpecialCoinSpawner(m_currencyDenomination) {};
private:
	static const uint32_t m_currencyDenomination = 10;
};

#endif  //!__SPECIAL10BRONZECOINSPAWNER__H__
