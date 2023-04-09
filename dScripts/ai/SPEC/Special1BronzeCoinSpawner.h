#ifndef __SPECIAL1BRONZECOINSPAWNER__H__
#define __SPECIAL1BRONZECOINSPAWNER__H__

#include "SpecialCoinSpawner.h"

class Special1BronzeCoinSpawner : public SpecialCoinSpawner {
public:
	Special1BronzeCoinSpawner() : SpecialCoinSpawner(m_currencyDenomination) {};
private:
	static const uint32_t m_currencyDenomination = 1;
};

#endif  //!__SPECIAL1BRONZECOINSPAWNER__H__


