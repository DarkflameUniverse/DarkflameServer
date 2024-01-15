#include "TimerTestScript.h"
#include "Entity.h"

void TimerTestScript::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "addTwoTimers") {
		self->AddTimer("expiredAddTwoTimers1", 10.0f);
		self->AddTimer("expiredAddTwoTimers2", 11.0f);
	}

	if (timerName == "addedBefore2") {
		self->AddTimer("expiredAddedBefore1", 12.0f);
		self->AddTimer("expiredAddedBefore2", 13.0f);
	}
}
