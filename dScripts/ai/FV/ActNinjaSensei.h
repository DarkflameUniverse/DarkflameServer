#pragma once
#include "CppScripts.h"

class ActNinjaSensei : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	std::string m_StudentGroup = "Sensei_kids";
	LOT m_StudentLOT = 2497;
};
