#pragma once
#ifndef FIREFIRSTSKILLONSTARTUP_H
#define FIREFIRSTSKILLONSTARTUP_H

#include "CppScripts.h"

class FireFirstSkillonStartup : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
};

#endif  //!FIREFIRSTSKILLONSTARTUP_H
