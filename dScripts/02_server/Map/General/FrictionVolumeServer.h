#ifndef __FRICTIONVOLUMESERVER__H__
#define __FRICTIONVOLUMESERVER__H__

#include "CppScripts.h"

class FrictionVolumeServer : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
private:
	const float DefaultFrictionAmount = 1.5f;
};

#endif  //!__FRICTIONVOLUMESERVER__H__
