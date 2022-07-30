#pragma once
#ifndef __FIREFIRSTSKILLONSTARTUP__H__
#define __FIREFIRSTSKILLONSTARTUP__H__

#include "CppScripts.h"

class FireFirstSkillonStartup : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
};

#endif  //!__FIREFIRSTSKILLONSTARTUP__H__
