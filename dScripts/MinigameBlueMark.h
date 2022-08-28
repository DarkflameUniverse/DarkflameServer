#pragma once
#include "CppScripts.h"

class MinigameBlueMark : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
private:
	const std::string m_Notification = "Blue_Mark";
};
