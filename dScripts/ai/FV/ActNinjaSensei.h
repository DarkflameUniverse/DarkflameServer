#pragma once
#include "CppScripts.h"

class ActNinjaSensei : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	std::string m_studentGroup = "Sensei_kids";
	LOT m_studentLOT = 2497;
	std::vector<Entity*> m_students = {};
};
