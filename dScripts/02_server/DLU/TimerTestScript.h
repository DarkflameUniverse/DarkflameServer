#ifndef __TIMERTESTSCRIPT__H__
#define __TIMERTESTSCRIPT__H__

#include "CppScripts.h"

class TimerTestScript : public CppScripts::Script {
public:
	void OnTimerDone(Entity* self, std::string timerName) override;
};

#endif  //!__TIMERTESTSCRIPT__H__
