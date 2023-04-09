#ifndef __SPECIAL25SILVERCOINSPAWNER__H__
#define __SPECIAL25SILVERCOINSPAWNER__H__

#include "SpecialCoinSpawner.h"

class Special25SilverCoinSpawner : public SpecialCoinSpawner {
public:
	Special25SilverCoinSpawner() : SpecialCoinSpawner(m_currencyDenomination) {};
private:
	static const uint32_t m_currencyDenomination = 2500;
};

#endif  //!__SPECIAL25SILVERCOINSPAWNER__H__
