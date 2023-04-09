#ifndef __SPECIAL25BRONZECOINSPAWNER__H__
#define __SPECIAL25BRONZECOINSPAWNER__H__

#include "SpecialCoinSpawner.h"

class Special25BronzeCoinSpawner : public SpecialCoinSpawner {
public:
	Special25BronzeCoinSpawner() : SpecialCoinSpawner(m_currencyDenomination) {};
private:
	static const uint32_t m_currencyDenomination = 25;
};

#endif  //!__SPECIAL25BRONZECOINSPAWNER__H__
