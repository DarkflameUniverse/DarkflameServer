#ifndef WBLROBOTCITIZEN_H
#define WBLROBOTCITIZEN_H

#include "CppScripts.h"

class WblRobotCitizen : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};

#endif  //!WBLROBOTCITIZEN_H
