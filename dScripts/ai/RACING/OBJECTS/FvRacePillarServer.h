#ifndef FVRACEPILLARSERVER__H
#define FVRACEPILLARSERVER__H

#include "CppScripts.h"

class FvRacePillarServer : public virtual CppScripts::Script {
protected:
	// Plays an animation on all entities in a group with a specific LOT
	void PlayAnimation(const std::string animName, const std::string group, const LOT lot);
};

#endif // FVRACEPILLARSERVER__H
