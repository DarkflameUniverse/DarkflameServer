#ifndef __WANDERINGVENDOR__H__
#define __WANDERINGVENDOR__H__

#include "CppScripts.h"

class WanderingVendor : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};

#endif  //!__WANDERINGVENDOR__H__
