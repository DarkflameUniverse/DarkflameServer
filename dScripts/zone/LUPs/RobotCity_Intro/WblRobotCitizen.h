#ifndef __WBLROBOTCITIZEN__H__
#define __WBLROBOTCITIZEN__H__

#include "CppScripts.h"

class WblRobotCitizen : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};

#endif  //!__WBLROBOTCITIZEN__H__
