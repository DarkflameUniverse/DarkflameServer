#ifndef __SPECIAL10SILVERCOINSPAWNER__H__
#define __SPECIAL10SILVERCOINSPAWNER__H__

#include "SpecialCoinSpawner.h"

class Special10SilverCoinSpawner : public SpecialCoinSpawner {
public:
	Special10SilverCoinSpawner() : SpecialCoinSpawner(m_currencyDenomination) {};
private:
	static const uint32_t m_currencyDenomination = 1000;
};

#endif  //!__SPECIAL10SILVERCOINSPAWNER__H__
