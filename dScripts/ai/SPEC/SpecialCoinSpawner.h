#pragma once
#include "CppScripts.h"

class SpecialCoinSpawner : public CppScripts::Script {
public:
	SpecialCoinSpawner(uint32_t CurrencyDenomination) {
		m_CurrencyDenomination = CurrencyDenomination;
	};
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, const std::string name, const std::string status) override;
private:
	int32_t m_CurrencyDenomination = 0;
};
