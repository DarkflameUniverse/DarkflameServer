#pragma once
#include "CppScripts.h"

#include <string>
#include <string_view>

class FvRaceDragon : public CppScripts::Script {
public:
	FvRaceDragon(const std::string_view lapAnimName, const int32_t lap) : m_LapAnimName(lapAnimName), m_Lap(lap) {}
private:
	void OnCollisionPhantom(Entity* self, Entity* target) override;
	std::string m_LapAnimName;
	int32_t m_Lap;
	LOT m_Dragon = 11898;
};
