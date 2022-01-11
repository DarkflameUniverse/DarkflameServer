#pragma once
#include "CppScripts.h"

class LegoDieRoll : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnTimerDone(Entity* self, std::string timerName);
private:
    constexpr static const float animTime = 2.0f;
	const std::u16string anims[6] = { u"roll-die-1", u"roll-die-2", u"roll-die-3", u"roll-die-4", u"roll-die-5", u"roll-die-6"};
};

